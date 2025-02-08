#ifndef ADSR_LIB
#define ADSR_LIB 1

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

enum envState { env_idle = 0, env_attack, env_decay, env_sustain, env_release };

typedef struct ADSR {
  float attack;   // seconds
  float decay;    // seconds
  float sustain;  // level
  float release;  // seconds
  float level;
  float level_attack;
  float level_release;
  float level_start;
  uint32_t sample_counter;
  float shape;
  float max;
  int32_t state;
  bool gate;
  float sample_rate;
} ADSR;

void ADSR_init(ADSR *adsr, float attack, float decay, float sustain,
               float release, float shape, float sample_rate) {
  adsr->attack = attack * sample_rate;  // convert s to samples
  adsr->level_attack = 0;
  adsr->decay = decay * sample_rate;  // convert s to samples
  adsr->sustain = sustain;
  adsr->release = release * sample_rate;  // convert s to samples
  adsr->state = env_idle;
  adsr->level = 0;
  adsr->level_start = 0;
  adsr->sample_counter = 0;
  adsr->gate = false;
  adsr->shape = shape;
  adsr->max = 1.0;
  adsr->sample_rate = sample_rate;
}

void ADSR_set_release(ADSR *adsr, float release) {
  adsr->release = release * adsr->sample_rate;
}

void ADSR_gate(ADSR *adsr, bool gate) {
  if (adsr->gate == gate) {
    return;
  }
  adsr->gate = gate;
  if (gate) {
    adsr->state = env_attack;
    adsr->level_start = adsr->level;  // Start from the current level
  } else {
    adsr->state = env_release;
  }
  adsr->sample_counter = 0;
}

float ADSR_process(ADSR *adsr) {
  adsr->sample_counter++;

  if (adsr->state == env_attack) {
    uint32_t elapsed = adsr->sample_counter;
    float curve_shape = adsr->attack / adsr->shape;
    adsr->level =
        adsr->level_start + (adsr->max - adsr->level_start) *
                                (1.0 - exp(-1.0 * (elapsed / curve_shape)));
    adsr->level_attack = adsr->level;
    adsr->level_release = adsr->level;
    if (elapsed >= adsr->attack) {
      adsr->state = env_decay;
      adsr->sample_counter = 0;
    }
  }

  if (adsr->state == env_decay) {
    uint32_t elapsed = adsr->sample_counter;
    if (elapsed >= adsr->decay) {
      adsr->state = env_sustain;
      adsr->sample_counter = 0;
    } else {
      float curve_shape = adsr->decay / adsr->shape;
      adsr->level = (adsr->sustain * adsr->max) +
                    (adsr->level_attack - (adsr->sustain * adsr->max)) *
                        exp(-1.0 * (elapsed / curve_shape));
      adsr->level_release = adsr->level;
    }
  }

  if (adsr->state == env_sustain) {
    // stay at the level
    // this prevents discontinuities when the decay is
    // over, which should get close to the adsr->sustain level
    // but sometimes not quite all the way
    // adsr->level = (adsr->sustain * adsr->max);
  }

  if (adsr->state == env_release) {
    uint32_t elapsed = adsr->sample_counter;
    if (adsr->level < 0.001) {
      adsr->state = env_idle;
      adsr->level = 0;
    } else {
      float curve_shape = adsr->release / adsr->shape;
      adsr->level = adsr->level_release * exp(-1.0 * (elapsed / curve_shape));
    }
  }

  // scale the level to the range [level_min, level_max]
  return adsr->level;
}

#endif
