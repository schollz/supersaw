/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <stdio.h>

#include "pico/audio_i2s.h"
#include "pico/stdlib.h"

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];

struct audio_buffer_pool *init_audio() {
  static audio_format_t audio_format = {
      .format = AUDIO_BUFFER_FORMAT_PCM_S16,
      .sample_freq = 44100,
      .channel_count = 2,
  };

  static struct audio_buffer_format producer_format = {.format = &audio_format,
                                                       .sample_stride = 4};

  struct audio_buffer_pool *producer_pool =
      audio_new_producer_pool(&producer_format, 3,
                              SAMPLES_PER_BUFFER);  // todo correct size
  bool __unused ok;
  const struct audio_format *output_format;
  struct audio_i2s_config config = {
      .data_pin = PICO_AUDIO_I2S_DATA_PIN,
      .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
      .dma_channel = 6,
      .pio_sm = 1,
  };

  output_format = audio_i2s_setup(&audio_format, &config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);
  audio_i2s_set_enabled(true);

  return producer_pool;
}

#include "adsr.h"
#include "verb.h"

const int block_size = 8192;

// create a saw wave struct
typedef struct LFSaw {
  float phase;
  float sample_rate;
  float amplitude;
  float phase_increment;
} LFSaw;

void LFSaw_init(LFSaw *saw, float freq, float sample_rate, float amplitude) {
  // choose random phase
  saw->phase = (float)rand() / RAND_MAX;
  saw->sample_rate = sample_rate;
  saw->amplitude = amplitude;
  saw->phase_increment = freq / sample_rate;
}

void LFSaw_set_freq(LFSaw *saw, float freq) {
  saw->phase_increment = freq / saw->sample_rate;
}

float __not_in_flash_func(LFSaw_next_sample)(LFSaw *saw) {
  float next = saw->phase * saw->amplitude;
  saw->phase += saw->phase_increment;
  if (saw->phase >= 1) {
    saw->phase -= 2;
  }
  return next;
}

typedef struct LFSaws {
  LFSaw saws[7];
} LFSaws;

float detuneCurve(float x) {
  return (10028.7312891634 * pow(x, 11)) - (50818.8652045924 * pow(x, 10)) +
         (111363.4808729368 * pow(x, 9)) - (138150.6761080548 * pow(x, 8)) +
         (106649.6679158292 * pow(x, 7)) - (53046.9642751875 * pow(x, 6)) +
         (17019.9518580080 * pow(x, 5)) - (3425.0836591318 * pow(x, 4)) +
         (404.2703938388 * pow(x, 3)) - (24.1878824391 * pow(x, 2)) +
         (0.6717417634 * x) + 0.0030115596;
}

float detuneAmounts[7] = {-0.11002313, -0.06288439, -0.01952356, 0,
                          0.01991221,  0.06216538,  0.10745242};

float amplitudeAmounts[7] = {0.5, 0.6, 0.7, 0.8, 0.7, 0.6, 0.5};
void LFSaws_init(LFSaws *saws, float freq, float sample_rate) {
  // generate random number between 0.4 and 0.6
  float detuneFactor = freq * detuneCurve(0.6);
  // print to stderr
  fprintf(stderr, "detuneFactor: %f\n", detuneFactor);
  for (int i = 0; i < 7; i++) {
    LFSaw_init(&saws->saws[i], freq + detuneFactor * detuneAmounts[i],
               sample_rate, amplitudeAmounts[i] / 4.0);
  }
}

float __not_in_flash_func(LFSaws_next_sample)(LFSaws *saws) {
  float next = 0;
  for (int i = 0; i < 7; i++) {
    next += LFSaw_next_sample(&saws->saws[i]);
  }
  return next;
}

typedef struct WhiteNoise {
  float amplitude;
} WhiteNoise;

void WhiteNoise_init(WhiteNoise *noise, float amplitude) {
  noise->amplitude = amplitude;
}

float WhiteNoise_next_sample(WhiteNoise *noise) {
  return ((float)rand() / RAND_MAX - 0.5) * noise->amplitude;
}

typedef struct OnePole {
  float prev_out;
} OnePole;

// out(i) = ((1 - abs(coef)) * in(i)) + (coef * out(i-1)).
float __not_in_flash_func(OnePole_next)(OnePole *self, float in, float coef) {
  float out = ((1 - fabs(coef)) * in) + (coef * self->prev_out);
  self->prev_out = out;
  return out;
}

typedef struct Voice {
  LFSaws saws;
  OnePole one_pole;
  ADSR adsr;
  WhiteNoise noise;
  float amp;
} Voice;

void Voice_init(Voice *voice, float freq, float amp, float sample_rate) {
  voice->amp = amp;
  // WhiteNoise_init(&voice->noise, 0.05);
  LFSaws_init(&voice->saws, freq, sample_rate);
  ADSR_init(&voice->adsr, 4, 1, 0.707, 0.5, 2.0, 44100);
}

float Voice_next_sample(Voice *voice) {
  float sample = 0;
  sample += LFSaws_next_sample(&voice->saws);
  sample += WhiteNoise_next_sample(&voice->noise);
  // generate random number between 0.97 and 0.99
  sample = OnePole_next(&voice->one_pole, sample, 0.9);
  sample = sample * ADSR_process(&voice->adsr);
  sample = sample * voice->amp;
  return sample;
}

void Voice_gate(Voice *voice, bool gate) { ADSR_gate(&voice->adsr, gate); }

void Voice_set_release(Voice *voice, float release) {
  ADSR_set_release(&voice->adsr, release);
}

int main() {
  stdio_init_all();

  for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
    sine_wave_table[i] =
        32767 * cosf(i * 2 * (float)(M_PI / SINE_WAVE_TABLE_LEN));
  }

  uint32_t step = 0x200000;
  uint32_t pos = 0;
  uint32_t pos2 = 0x8000;  // Introduce phase difference for stereo effect
  uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
  uint vol = 128;

  // reverb
  struct sDattorroVerb *verb = DattorroVerb_create();
  // giant reverb
  DattorroVerb_setPreDelay(verb, 0.2);
  DattorroVerb_setPreFilter(verb, 0.9);
  DattorroVerb_setInputDiffusion1(verb, 0.85);
  DattorroVerb_setInputDiffusion2(verb, 0.75);
  DattorroVerb_setDecayDiffusion(verb, 0.5);
  DattorroVerb_setDecay(verb, 0.9);
  DattorroVerb_setDamping(verb, 0.3);

#define NUM_VOICES 3
  Voice voice[NUM_VOICES];
  // overtone series
  float freqs[7] = {111, 219, 441, 55, 1760, 3520, 7040};
  float amps[7] = {0.75, 0.5, 0.25, 0.25, 0.125, 0.0625, 0.03125};
  for (int i = 0; i < NUM_VOICES; i++) {
    Voice_init(&voice[i], freqs[i], amps[i], 44100);
    Voice_gate(&voice[i], true);
    Voice_set_release(&voice[i], 0.1);
  }
  struct audio_buffer_pool *ap = init_audio();

  uint64_t end_time = 0;
  uint64_t start_time = 0;
  while (true) {
    int c = getchar_timeout_us(0);
    if (c >= 0) {
      if (c == '-' && vol) {
        for (int i = 0; i < NUM_VOICES; i++) Voice_gate(&voice[i], false);
      }
      if (c == '=' || c == '+') {
        // start all voices
        for (int i = 0; i < NUM_VOICES; i++) Voice_gate(&voice[i], true);
      }

      float percent_audio_block =
          (float)(start_time - end_time) / (float)SAMPLES_PER_BUFFER;
      printf("vol = %d, step = %d %2.1f     \r", vol, step >> 16,
             percent_audio_block);
    }
    struct audio_buffer *buffer = take_audio_buffer(ap, true);
    int16_t *samples = (int16_t *)buffer->buffer->bytes;
    end_time = time_us_64();
    for (uint i = 0; i < buffer->max_sample_count * 2; i += 2) {
      float sample = 0;
      for (int j = 0; j < NUM_VOICES; j++)
        sample += Voice_next_sample(&voice[j]) / NUM_VOICES;
      if (i == 44100 * 5) {
        for (int j = 0; j < NUM_VOICES; j++) Voice_gate(&voice[j], false);
      }
      // DattorroVerb_process(verb, sample);
      // float sampleL = DattorroVerb_getLeft(verb);
      // float sampleR = DattorroVerb_getRight(verb);
      samples[i] = (int16_t)(sample * 32767);
      samples[i + 1] = (int16_t)(sample * 32767);
      // samples[i] = sampleL(vol * sine_wave_table[pos >> 16u]) >> 8u;
      // samples[i + 1] = (vol * sine_wave_table[pos >> 16u]) >> 8u;
      // pos += step;
      // if (pos >= pos_max) pos -= pos_max;
    }
    start_time = time_us_64();
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(ap, buffer);
  }
  puts("\n");
  return 0;
}
