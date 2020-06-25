/*
  robin.h - v0.1
  MIDI frequency modulation synthesizer
*/

#ifndef ROBIN_H
#define ROBIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RBN_STATIC
#define RBNDEF static
#else
#define RBNDEF extern
#endif

#ifndef RBN_CHAN_COUNT
#define RBN_CHAN_COUNT 1
#endif

#ifndef RBN_PROGRAM_COUNT
#define RBN_PROGRAM_COUNT 1
#endif

#ifndef RBN_VOICE_COUNT
#define RBN_VOICE_COUNT 64
#endif

#ifndef RBN_OPERATOR_COUNT
#define RBN_OPERATOR_COUNT 8
#endif

#ifndef RBN_ENVPT_COUNT
#define RBN_ENVPT_COUNT 6
#endif

#ifndef RBN_FILTER_COUNT
#define RBN_FILTER_COUNT 4
#endif

#ifndef RBN_BLOCK_SAMPLES
#define RBN_BLOCK_SAMPLES 64
#endif

  typedef enum rbn_result {
    rbn_success,
    rbn_unknown_msg_type,
    rbn_unknown_control,
    rbn_unknown_sample_format,
    rbn_out_of_voice,
    rbn_nonblock_render_size,
  } rbn_result;

  typedef enum rbn_sample_format {
    rbn_s16,
  } rbn_sample_format;

  typedef enum rbn_msg_type {
    rbn_end_of_track = 0x2f,
    rbn_set_tempo = 0x51,
    rbn_note_off = 0x80,
    rbn_note_on = 0x90,
    rbn_key_pressure = 0xa0,
    rbn_control_change = 0xb0,
    rbn_program_change = 0xc0,
    rbn_channel_pressure = 0xd0,
    rbn_pitch_bend = 0xe0,
  } rbn_msg_type;

  typedef enum rbn_control {
    rbn_bank_select,
    rbn_modulation_wheel,
    rbn_breath,
    rbn_volume = 7,
    rbn_balance,
    rbn_pan = 10,
    rbn_expression,
    rbn_nrpn = 98,
    rbn_rpn = 100,
    rbn_max_controls,
  } rbn_control;

  typedef enum rbn_filter_type {
    rbn_filter_none,
    rbn_filter_lowpass,
    rbn_filter_highpass,
  } rbn_filter_type;

  typedef struct rbn_envelope {
    struct {
      float time, value;
    } points[RBN_ENVPT_COUNT];

    // Linear ramp to zero after sustain
    // 0 means instantaneous
    // -1 means stay at sustain value
    float release_time;
  } rbn_envelope;

  typedef struct rbn_operator {
    rbn_envelope volume_envelope;
    rbn_envelope pitch_envelope;
    float freq_offset;
    float freq_ratio;
    float noise;
    float output;
  } rbn_operator;

  typedef struct rbn_filter {
    rbn_filter_type type;
    float cutoff;
  } rbn_filter;

  typedef struct rbn_program {
    rbn_operator operators[RBN_OPERATOR_COUNT];
    rbn_filter filters[RBN_FILTER_COUNT];
    float op_matrix[RBN_OPERATOR_COUNT][RBN_OPERATOR_COUNT];
    float fil_matrix[RBN_OPERATOR_COUNT][RBN_FILTER_COUNT];

    // Cached
    uint64_t sustain_samples;
    uint64_t release_samples;
  } rbn_program;

  typedef struct rbn_voice {
    float phases[RBN_OPERATOR_COUNT];
    float values[RBN_OPERATOR_COUNT];
    float volumes[RBN_OPERATOR_COUNT];
    float pitches[RBN_OPERATOR_COUNT];
    uint64_t press_index;
    uint64_t release_index;
    uint64_t inactive_index;
    rbn_program* program;
    float base_freq_rate;
    uint8_t channel;
    uint8_t key;
    uint8_t velocity;
  } rbn_voice;

  typedef struct rbn_channel {
    float volume[2];
    float pan;
    float pitch_bend;
    uint8_t program;
    uint8_t controls[rbn_max_controls];
  } rbn_channel;

  typedef union rbn_msg {
    uint32_t u32;
    uint8_t u8[4];
    struct {
      uint8_t channel;
      uint8_t type;
      union {
        uint8_t key;
        uint8_t instrument;
        uint8_t control;
      };
      union {
        uint8_t velocity;
        uint8_t value;
      };
    };
  } rbn_msg;

  typedef struct rbn_config {
    uint32_t sample_rate;
    rbn_sample_format sample_format;
  } rbn_config;

  typedef struct rbn_instance {
    rbn_config config;
    uint64_t sample_index;
    uint64_t rendered_samples;

    float dynamic_range;

    rbn_channel channels[RBN_CHAN_COUNT];
    rbn_program programs[RBN_PROGRAM_COUNT];
    rbn_voice voices[RBN_VOICE_COUNT];

    // Cached
    float inv_sample_rate;
  } rbn_instance;


  RBNDEF rbn_result rbn_init(rbn_instance* inst, const rbn_config* config);
  RBNDEF rbn_result rbn_shutdown(rbn_instance* inst);
  RBNDEF rbn_result rbn_refresh(rbn_instance* inst);
  RBNDEF rbn_result rbn_render(rbn_instance* inst, void* samples, uint64_t sample_count);

  RBNDEF rbn_result rbn_send_msg(rbn_instance* inst, rbn_msg msg);
  RBNDEF rbn_result rbn_play_note(rbn_instance* inst, uint8_t channel, uint8_t key, uint8_t velocity);
  RBNDEF rbn_result rbn_stop_note(rbn_instance* inst, uint8_t channel, uint8_t key);
  RBNDEF rbn_result rbn_stop_all_notes(rbn_instance* inst);
  RBNDEF rbn_result rbn_set_program(rbn_instance* inst, uint8_t channel, uint8_t program);
  RBNDEF rbn_result rbn_set_pitch_bend(rbn_instance* inst, uint8_t channel, float value);

#ifdef __cplusplus
}
#endif
#endif // ROBIN_H

#ifdef RBN_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

#define RBN_PI 3.1415f
#define RBN_TAU 6.2832f
#define RBN_INV_TAU (1.f/6.2832f)

#ifndef RBN_RAND
#include <stdlib.h>
#define RBN_RAND() (((float)rand() / (RAND_MAX / 2)) - 1.f)
#endif

#ifndef RBN_MEMCPY
#include <string.h>
#define RBN_MEMCPY memcpy
#endif

#ifndef RBN_MEMSET
#include <string.h>
#define RBN_MEMSET memset
#endif

  static float rbn_max(float a, float b) {
    return a > b ? a : b;
  }

  static void rbn_channel_update_volumes(rbn_channel* channel) {
    const float channel_volume = (channel->controls[rbn_volume] / 126.f) * (channel->controls[rbn_expression] / 126.f);
    const float pan = channel->controls[rbn_pan] / 126.f;
    channel->volume[0] = cosf((RBN_PI / 2.f) * pan);
    channel->volume[1] = sinf((RBN_PI / 2.f) * pan);
  }

  static void rbn_voice_compute_base_freq_rate(const rbn_instance* inst, rbn_voice* voice) {
    const rbn_channel* channel = inst->channels + voice->channel;
    const float key = (voice->key - 69.f) + channel->pitch_bend;
    const float frequency = 440.f * powf(2.f, key / 12.f);
    voice->base_freq_rate = frequency / inst->config.sample_rate;
  }

  static void rbn_compute_envelope(const rbn_instance* inst, const rbn_voice* voice, const rbn_envelope* envelope, float current, float* rate) {
    float sustain_value = 0.f;
    float sustain_time = 0.f;
    const float press_time = (float)(inst->sample_index - voice->press_index) * inst->inv_sample_rate;
    const int has_released = voice->release_index != UINT64_MAX && voice->release_index <= inst->sample_index;

    for(uintptr_t i = 0; i < RBN_ENVPT_COUNT; i++) {
      if(!has_released && envelope->points[i].time >= press_time) {
        const float next_time = envelope->points[i].time;
        const float next_value = envelope->points[i].value;
        const float time_to_next = next_time - press_time;

        *rate = (next_value - current) / rbn_max(time_to_next * inst->config.sample_rate, RBN_BLOCK_SAMPLES);
        return;
      }

      if(i == 0 || envelope->points[i].time > 0.f) {
        sustain_value = envelope->points[i].value;
        sustain_time = envelope->points[i].time;
      } else {
        break;
      }
    }

    if(!has_released || envelope->release_time < 0.f) {
      *rate = (sustain_value - current) / RBN_BLOCK_SAMPLES;
      return;
    }

    if(envelope->release_time > 0.f) {
      const float release_time = (float)(inst->sample_index - voice->release_index) * inst->inv_sample_rate;
      const float time_to_zero = envelope->release_time - release_time;
      *rate = -current / rbn_max(time_to_zero * inst->config.sample_rate, RBN_BLOCK_SAMPLES);
    } else { // Go to zero
      *rate = -current / RBN_BLOCK_SAMPLES;
    }
  }

  static rbn_result rbn_render_voice_block(rbn_instance* inst, rbn_voice* voice, rbn_channel* channel, float* samples) {
    float values[RBN_OPERATOR_COUNT];
    float volume_rates[RBN_OPERATOR_COUNT];
    float pitch_rates[RBN_OPERATOR_COUNT];

    const float base_freq_rate = voice->base_freq_rate;
    const float velocity = voice->velocity / 127.f;

    const rbn_program* program = voice->program;
    const rbn_operator* operators = program->operators;
    float* volumes = voice->volumes;
    float* pitches = voice->pitches;

    for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
      rbn_compute_envelope(inst, voice, &operators[i].volume_envelope, volumes[i], volume_rates + i);
      rbn_compute_envelope(inst, voice, &operators[i].pitch_envelope, pitches[i], pitch_rates + i);
    }

    for(uintptr_t i = 0; i < RBN_BLOCK_SAMPLES; i++) {
      for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
        float phase = voice->phases[j];
        for(uintptr_t k = 0; k < RBN_OPERATOR_COUNT; k++) {
          phase += program->op_matrix[k][j] * voice->values[k] * RBN_INV_TAU;
        }

        const float noisef = program->operators[j].noise;
        const float noise = RBN_RAND();
        values[j] = (sinf(phase * RBN_TAU) * (1.f - noisef) + noise * noisef) * volumes[j];
        volumes[j] += volume_rates[j];
      }

      for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
        const float value = values[j] * operators[j].output * velocity;
        samples[i * 2 + 0] += value * channel->volume[0];
        samples[i * 2 + 1] += value * channel->volume[1];

        voice->phases[j] += base_freq_rate * operators[j].freq_ratio * powf(2.f, pitches[j]);
        voice->values[j] = values[j];
        pitches[j] += pitch_rates[j];
      }
    }

    for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
      voice->phases[i] = fmodf(voice->phases[i], 1.f);
    }

    inst->rendered_samples += RBN_BLOCK_SAMPLES;
    return rbn_success;
  }

  static rbn_result rbn_render_block(rbn_instance* inst, float* samples) {
    for(uintptr_t v = 0; v < RBN_VOICE_COUNT; v++) {
      rbn_voice* voice = inst->voices + v;
      if(voice->inactive_index > inst->sample_index) {
        rbn_render_voice_block(inst, voice, inst->channels + voice->channel, samples);
      }
    }
    return rbn_success;
  }

  rbn_result rbn_init(rbn_instance* inst, const rbn_config* config) {
    RBN_MEMSET(inst, 0, sizeof(rbn_instance));
    RBN_MEMCPY(&inst->config, config, sizeof(*config));

    inst->dynamic_range = 1.f;
    inst->inv_sample_rate = 1.f / config->sample_rate;

    for(uintptr_t i = 0; i < RBN_CHAN_COUNT; i++) {
      rbn_channel* channel = inst->channels + i;
      channel->controls[rbn_volume] = 127; // Full volume
      channel->controls[rbn_expression] = 127; // Full expression
      channel->controls[rbn_balance] = 64; // Centered balance
      channel->controls[rbn_pan] = 64; // Centered pan
      rbn_channel_update_volumes(channel);
    }

    return rbn_refresh(inst);
  }

  rbn_result rbn_shutdown(rbn_instance* inst) {
    return rbn_success;
  }

  rbn_result rbn_refresh(rbn_instance* inst) {
    for(uintptr_t i = 0; i < RBN_PROGRAM_COUNT; i++) {
      rbn_program* program = inst->programs + i;
      program->sustain_samples = 0;
      program->release_samples = 0;
      for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
        rbn_operator* operator = program->operators + j;

        // Compute max release time in samples
        if(operator->volume_envelope.release_time > 0.f) {
          uint64_t release_samples = (uint64_t)(operator->volume_envelope.release_time * inst->config.sample_rate);
          if(release_samples > program->release_samples) {
            program->release_samples = release_samples;
          }
        }

        // Compute max time to sustain in samples
        for(uintptr_t k = 0; k < RBN_ENVPT_COUNT; k++) {
          if(operator->volume_envelope.points[k].time > 0.f) {
            uint64_t sustain_samples = (uint64_t)(operator->volume_envelope.points[k].time * inst->config.sample_rate);
            if(sustain_samples > program->sustain_samples) {
              program->sustain_samples = sustain_samples;
            }
          }
        }
      }
    }

    return rbn_success;
  }

  rbn_result rbn_render(rbn_instance* inst, void* samples, uint64_t sample_count) {
    if(sample_count % RBN_BLOCK_SAMPLES != 0) {
      return rbn_nonblock_render_size;
    }
    for(uintptr_t i = 0; i < sample_count; i += RBN_BLOCK_SAMPLES) {
      float fsamples[RBN_BLOCK_SAMPLES * 2] = {0};
      rbn_result result = rbn_render_block(inst, fsamples);
      if(result == rbn_success) {
        // Convert float to output format
        int16_t* isamples = (int16_t*)samples + i * 2;
        switch(inst->config.sample_format) {
          case rbn_s16:
            for(uintptr_t j = 0; j < RBN_BLOCK_SAMPLES * 2; j++) {
              const float range = fabs(fsamples[j]) * 1.01f;
              if(range > inst->dynamic_range) {
                inst->dynamic_range = range;
              }
              isamples[j] = (int16_t)((fsamples[j] / inst->dynamic_range) * 0x8000);
            }
            break;
          default:
            return rbn_unknown_sample_format;
        }
      } else {
        return result;
      }
      inst->sample_index += RBN_BLOCK_SAMPLES;
    }
    return rbn_success;
  }

  rbn_result rbn_send_msg(rbn_instance* inst, rbn_msg msg) {
    rbn_channel* channel = inst->channels + msg.channel;
    switch(msg.type) {
      case rbn_end_of_track:
      case rbn_set_tempo: break; // Ignore
      case rbn_note_off:
        return rbn_stop_note(inst, msg.channel, msg.key);
      case rbn_note_on:
        if(msg.velocity > 0) {
          return rbn_play_note(inst, msg.channel, msg.key, msg.velocity);
        } else {
          return rbn_stop_note(inst, msg.channel, msg.key);
        }
      case rbn_control_change:
        //printf("%d %d %d %d\n", msg.channel, msg.type, msg.key, msg.velocity);
        if(msg.control < rbn_max_controls) {
          channel->controls[msg.control] = msg.value;
          switch(msg.control) {
            case rbn_volume: case rbn_expression: case rbn_pan: case rbn_balance:
              rbn_channel_update_volumes(channel);
              break;
          }
        } else {
          return rbn_unknown_control;
        }
        break;
      case rbn_program_change: rbn_set_program(inst, msg.channel, msg.instrument); break;
      case rbn_pitch_bend: return rbn_set_pitch_bend(inst, msg.channel, ((msg.u8[2] | (msg.u8[3] << 7)) - 0x2000) / (float)0x1000); break;
      default:
        //printf("%d %d %d %d\n", msg.channel, msg.type, msg.key, msg.velocity);
        return rbn_unknown_msg_type;
    }
    return rbn_success;
  }

  rbn_result rbn_play_note(rbn_instance* inst, uint8_t channel, uint8_t key, uint8_t velocity) {
    for(uintptr_t i = 0; i < RBN_VOICE_COUNT; i++) {
      rbn_voice* voice = inst->voices + i;
      const int active = voice->inactive_index > inst->sample_index;
      if(!active) {
        voice->inactive_index = UINT64_MAX;
        voice->press_index = inst->sample_index;
        voice->release_index = UINT64_MAX;
        RBN_MEMSET(voice->phases, 0, sizeof(float) * RBN_OPERATOR_COUNT);
        RBN_MEMSET(voice->values, 0, sizeof(float) * RBN_OPERATOR_COUNT);
        RBN_MEMSET(voice->volumes, 0, sizeof(float) * RBN_OPERATOR_COUNT);
        RBN_MEMSET(voice->pitches, 0, sizeof(float) * RBN_OPERATOR_COUNT);
        voice->program = inst->programs + inst->channels[channel].program;
        voice->channel = channel;
        voice->key = key;
        voice->velocity = velocity;
#ifdef RBN_GENERAL_MIDI // TODO: Make this more generic?
        if(channel == 9) { // Percussions
          voice->key = 60;
          voice->program = inst->programs + 93 + key;
          // Percussions are instantly off
          voice->release_index = voice->press_index + voice->program->sustain_samples;
          voice->inactive_index = voice->release_index + voice->program->release_samples;
        }
#endif
        rbn_voice_compute_base_freq_rate(inst, voice);
        return rbn_success;
      }
    }
    // Unable to play note, panic?
    __debugbreak();
    return rbn_out_of_voice;
  }

  rbn_result rbn_stop_note(rbn_instance* inst, uint8_t channel, uint8_t key) {
    for(uintptr_t i = 0; i < RBN_VOICE_COUNT; i++) {
      rbn_voice* voice = inst->voices + i;
      const int active = voice->inactive_index > inst->sample_index;
      if(active && voice->channel == channel && voice->key == key) {
        voice->release_index = inst->sample_index;
        voice->inactive_index = inst->sample_index + voice->program->release_samples;
      }
    }
    return rbn_success;
  }

  rbn_result rbn_stop_all_notes(rbn_instance* inst) {
    RBN_MEMSET(inst->voices, 0, RBN_VOICE_COUNT * sizeof(rbn_voice));
    return rbn_success;
  }

  rbn_result rbn_set_program(rbn_instance* inst, uint8_t channel, uint8_t program) {
    inst->channels[channel].program = program;
    return rbn_success;
  }

  rbn_result rbn_set_pitch_bend(rbn_instance* inst, uint8_t channel, float value) {
    inst->channels[channel].pitch_bend = value;
    for(uintptr_t i = 0; i < RBN_VOICE_COUNT; i++) {
      rbn_voice* voice = inst->voices + i;
      const int active = voice->inactive_index > inst->sample_index;
      // Update pitch bend for active unreleased voices
      if(voice->channel == channel && active && voice->release_index == UINT64_MAX) {
        rbn_voice_compute_base_freq_rate(inst, voice);
      }
    }
    return rbn_success;
  }
#ifdef __cplusplus
}
#endif

#endif // RBN_IMPLEMENTATION

/*

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>

*/
