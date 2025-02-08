#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"
//
// No debugging
#define debug(f_, ...)
#define debugp(f_)
#define debug_pin(pin_, val_)
//
#include "lib/computer.h"
//
#include "lib/audio_worker.h"
#include "lib/clock.h"
#include "lib/divider.h"

clock clk[2];
divider pulseout1_divider, pulseout2_divider, cvout1_divider, cvout2_divider,
    tm_divider, bg_divider;

int main() {
  stdio_init_all();

  // Run at 144MHz, mild overclock
  set_sys_clock_khz(144000, true);

  adc_run(false);
  adc_select_input(0);

  sleep_ms(50);
  SetupComputerIO();

  clock_init(&clk[0]);
  clock_init(&clk[1]);
  divider_init(&pulseout1_divider);
  divider_init(&pulseout2_divider);
  divider_init(&cvout1_divider);
  divider_init(&cvout2_divider);
  divider_init(&tm_divider);
  divider_init(&bg_divider);

  multicore_launch_core1(audio_worker);

  while (true) {
    printf("ok\n");
    sleep_ms(1000);
  }
}
