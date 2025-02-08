#include <malloc.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint32_t getTotalHeap() {
  extern char __StackLimit, __bss_end__;

  return &__StackLimit - &__bss_end__;
}

uint32_t getFreeHeap() {
  struct mallinfo m = mallinfo();

  return getTotalHeap() - m.uordblks;
}

void print_memory_usage() {
  uint32_t total_heap = getTotalHeap();
  uint32_t used_heap = total_heap - getFreeHeap();
  printf("memory usage: %2.1f%% (%ld/%ld)\n",
         (float)(used_heap) / (float)(total_heap) * 100.0, used_heap,
         total_heap);
}

#include "pico/stdlib.h"

#define ONBOARD_LED 25

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

float amplitudeAmounts[7] = {0.5, 0.3, 0.4, 0.8, 0.8, 0.4, 0.3};
void LFSaws_init(LFSaws *saws, float freq, float sample_rate) {
  float detuneFactor = freq * detuneCurve(0.5);
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
  OnePole one_pole;
  ADSR_init(&voice->adsr, 4, 1, 0.707, 0.5, 2.0, 48000);
}

float Voice_next_sample(Voice *voice) {
  float sample = 0;
  sample += LFSaws_next_sample(&voice->saws);
  sample += WhiteNoise_next_sample(&voice->noise);
  // generate random number between 0.97 and 0.99
  float random = (float)rand() / RAND_MAX * 0.15 + 0.8;
  sample = OnePole_next(&voice->one_pole, sample, random);
  sample = sample * ADSR_process(&voice->adsr);
  sample = sample * voice->amp;
  return sample;
}

void Voice_gate(Voice *voice, bool gate) { ADSR_gate(&voice->adsr, gate); }

void Voice_set_release(Voice *voice, float release) {
  ADSR_set_release(&voice->adsr, release);
}

float buffer[1000];

int main() {
  stdio_init_all();
  gpio_init(ONBOARD_LED);
  gpio_set_dir(ONBOARD_LED, GPIO_OUT);

  sleep_ms(1000);
  printf("begin\n");
  sleep_ms(1000);

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
  float freqs[NUM_VOICES] = {110, 220, 440, 55, 1760, 3520, 7040};
  float amps[NUM_VOICES] = {0.75, 0.5, 0.25, 0.25, 0.125, 0.0625, 0.03125};
  for (int i = 0; i < NUM_VOICES; i++) {
    Voice_init(&voice[i], freqs[i], amps[i], 48000);
    Voice_gate(&voice[i], true);
    Voice_set_release(&voice[i], 0.1);
  }

  // print total memory

  while (true) {
    print_memory_usage();
    gpio_put(ONBOARD_LED, 1);
    sleep_ms(500);
    gpio_put(ONBOARD_LED, 0);
    sleep_ms(500);

    float total_samples = 48000;
    // start time
    uint64_t start_time = time_us_64();
    for (int i = 0; i < total_samples; i++) {
      float sample = 0;
      for (int j = 0; j < NUM_VOICES; j++)
        sample += Voice_next_sample(&voice[j]) / NUM_VOICES;
      if (i == 48000 * 5) {
        for (int j = 0; j < NUM_VOICES; j++) Voice_gate(&voice[j], false);
      }
      buffer[i % 1000] = sample;
    }
    // end time
    uint64_t end_time = time_us_64();
    float total_time = (float)(end_time - start_time);
    float us_per_voice =
        (float)(end_time - start_time) / total_samples / NUM_VOICES;

    // start time
    start_time = time_us_64();
    for (int i = 0; i < total_samples; i++) {
      float sample = 0;
      for (int j = 0; j < NUM_VOICES; j++)
        sample += Voice_next_sample(&voice[j]) / NUM_VOICES;
      if (i == 48000 * 5) {
        for (int j = 0; j < NUM_VOICES; j++) Voice_gate(&voice[j], false);
      }
      DattorroVerb_process(verb, sample);
      float sampleL = DattorroVerb_getLeft(verb);
      float sampleR = DattorroVerb_getRight(verb);
      buffer[i % 1000] = sampleL + sampleR;
    }
    // end time
    end_time = time_us_64();
    float us_per_reverb =
        ((end_time - start_time) - total_time) / total_samples;
    printf("us per voice: %2.1f\n", us_per_voice);
    printf("us per reverb: %2.1f\n", us_per_reverb);
    printf("%% of block: %2.1f%%\n", ((end_time - start_time) / total_samples) /
                                         (1000000.0f / 48000.0f) * 100.0f);
  }
}