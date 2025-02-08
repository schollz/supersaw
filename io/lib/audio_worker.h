
////////////////////////////////////////
// Audio core functions
void process_sample();

// The ADC (/DMA) run mode, used to stop DMA in a known state before writing to
// flash
#define RUN_ADC_MODE_RUNNING 0
#define RUN_ADC_MODE_REQUEST_ADC_STOP 1
#define RUN_ADC_MODE_ADC_STOPPED 2
#define RUN_ADC_MODE_REQUEST_ADC_RESTART 3
volatile uint8_t runADCMode = RUN_ADC_MODE_RUNNING;
// Buffers that DMA reads into / out of
uint16_t ADC_Buffer[2][8];
uint16_t SPI_Buffer[2][2];
uint8_t adc_dma, spi_dma;  // DMA ids
uint8_t dmaPhase = 0;
volatile int16_t dacOutL = 0, dacOutR = 0;
volatile int16_t adcInL = 0x800, adcInR = 0x800;

// Convert signed int16 value into data string for DAC output
uint16_t __not_in_flash_func(dacval)(int16_t value, uint16_t dacChannel) {
  return (dacChannel | 0x3000) |
         (((uint16_t)((value & 0x0FFF) + 0x800)) & 0x0FFF);
}

// Per-audio-sample ISR, called when two sets of ADC samples have been collected
// from all four inputs
void __not_in_flash_func(buffer_full)() {
  debug_pin(DEBUG_2, true);
  static int mux_state = 0;
  static int norm_probe_count = 0;
  static int np = 0, np1 = 0, np2 = 0;

  adc_select_input(0);

  // Set up new writes into next buffer
  uint8_t cpuPhase = dmaPhase;
  dmaPhase = 1 - dmaPhase;

  dma_hw->ints0 = 1u << adc_dma;  // reset adc interrupt flag
  dma_channel_set_write_addr(adc_dma, ADC_Buffer[dmaPhase],
                             true);  // start writing into new buffer
  dma_channel_set_read_addr(spi_dma, SPI_Buffer[dmaPhase],
                            true);  // start reading from new buffer

  ////////////////////////////////////////
  // Collect various inputs and put them in variables for the DSP

  // Set audio inputs, by averaging the two samples collected
  adcInR = ((ADC_Buffer[cpuPhase][0] + ADC_Buffer[cpuPhase][4]) - 0x1000) >> 1;

  adcInL = ((ADC_Buffer[cpuPhase][1] + ADC_Buffer[cpuPhase][5]) - 0x1000) >> 1;

  ////////////////////////////////////////
  // Run the DSP
  process_sample();
  SPI_Buffer[cpuPhase][0] = dacval(dacOutL, DAC_CHANNEL_A);
  SPI_Buffer[cpuPhase][1] = dacval(dacOutR, DAC_CHANNEL_B);

  // Indicate to usb core that we've finished running this sample.
  if (runADCMode == RUN_ADC_MODE_REQUEST_ADC_STOP) {
    adc_run(false);
    adc_set_round_robin(0);
    adc_select_input(0);

    runADCMode = RUN_ADC_MODE_ADC_STOPPED;
  }
}

// Main audio core function
void __not_in_flash_func(audio_worker)() {
  // Audio worker must be able to stop when USB worker wants to write to flash
  multicore_lockout_victim_init();

  adc_select_input(0);
  adc_set_round_robin(0b0001111U);

  // enabled, with DMA request when FIFO contains data, no erro flag, no byte
  // shift
  adc_fifo_setup(true, true, 1, false, false);

  // ADC clock runs at 48MHz
  // 48MHz ÷ (124+1) = 384kHz ADC sample rate
  //                 = 8×48kHz audio sample rate
  adc_set_clkdiv(124);

  // claim and setup DMAs for reading to ADC, and writing to SPI DAC
  adc_dma = dma_claim_unused_channel(true);
  spi_dma = dma_claim_unused_channel(true);

  dma_channel_config adc_dmacfg, spi_dmacfg;
  adc_dmacfg = dma_channel_get_default_config(adc_dma);
  spi_dmacfg = dma_channel_get_default_config(spi_dma);

  // Reading from ADC into memory buffer, so increment on write, but no
  // increment on read
  channel_config_set_transfer_data_size(&adc_dmacfg, DMA_SIZE_16);
  channel_config_set_read_increment(&adc_dmacfg, false);
  channel_config_set_write_increment(&adc_dmacfg, true);

  // Synchronise ADC DMA the ADC samples
  channel_config_set_dreq(&adc_dmacfg, DREQ_ADC);

  // Setup DMA for 8 ADC samples
  dma_channel_configure(adc_dma, &adc_dmacfg, ADC_Buffer[dmaPhase],
                        &adc_hw->fifo, 8, true);

  // Turn on IRQ for ADC DMA
  dma_channel_set_irq0_enabled(adc_dma, true);

  // Call buffer_full ISR when ADC DMA finished
  irq_set_enabled(DMA_IRQ_0, true);
  irq_set_exclusive_handler(DMA_IRQ_0, buffer_full);

  // Set up DMA for SPI
  spi_dmacfg = dma_channel_get_default_config(spi_dma);
  channel_config_set_transfer_data_size(&spi_dmacfg, DMA_SIZE_16);

  // SPI DMA timed to SPI TX
  channel_config_set_dreq(&spi_dmacfg, SPI_DREQ);

  // Set up DMA to transmit 2 samples to SPI
  dma_channel_configure(spi_dma, &spi_dmacfg, &spi_get_hw(SPI_PORT)->dr, NULL,
                        2, false);

  adc_run(true);

  while (1) {
    // If ready to restart
    if (runADCMode == RUN_ADC_MODE_REQUEST_ADC_RESTART) {
      runADCMode = RUN_ADC_MODE_RUNNING;

      dma_hw->ints0 = 1u << adc_dma;  // reset adc interrupt flag
      dma_channel_set_write_addr(adc_dma, ADC_Buffer[dmaPhase],
                                 true);  // start writing into new buffer
      dma_channel_set_read_addr(spi_dma, SPI_Buffer[dmaPhase],
                                true);  // start reading from new buffer

      adc_set_round_robin(0);
      adc_select_input(0);
      adc_set_round_robin(0b0001111U);
      adc_run(true);
    }
  }
}

// process_sample is called once-per-audio-sample (48kHz) by the buffer_full ISR
const int startupSampleDelay = 20000;
void __not_in_flash_func(process_sample)() {
  dacOutL = adcInL;
  dacOutR = adcInR;
}