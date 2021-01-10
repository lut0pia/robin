#include "rbncli.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const size_t sample_size = sizeof(uint16_t) * 2;

typedef struct rbncli_sample_buffer {
  uintptr_t index;
  int16_t samples[RBN_BLOCK_SAMPLES * 2];
} rbncli_sample_buffer;

static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 sample_count) {
  rbncli_sample_buffer* buffer = (rbncli_sample_buffer*)device->pUserData;
  rbncli_lock();
  while(sample_count > 0) {
    size_t advance_sample_count;
    if(buffer->index > 0) {
      advance_sample_count = RBN_BLOCK_SAMPLES - buffer->index;
      assert(sample_count >= advance_sample_count);
      memcpy(output, buffer->samples + buffer->index * 2, advance_sample_count * sample_size);
      buffer->index = 0;
    } else if(sample_count < RBN_BLOCK_SAMPLES) {
      rbn_render(&inst, buffer->samples, RBN_BLOCK_SAMPLES);
      advance_sample_count = sample_count;
      memcpy(output, buffer->samples + buffer->index * 2, advance_sample_count * sample_size);
      buffer->index = sample_count;
    } else {
      advance_sample_count = (sample_count / RBN_BLOCK_SAMPLES) * RBN_BLOCK_SAMPLES;
      rbn_render(&inst, output, advance_sample_count);
    }
    sample_count -= advance_sample_count;
    output = (uint8_t*)output + advance_sample_count * sample_size;
  }
  rbncli_unlock();
}

int rbncli_init_ma_device(ma_device* device) {
  ma_device_config device_config;
  device_config = ma_device_config_init(ma_device_type_playback);
  device_config.playback.format = ma_format_s16;
  device_config.playback.channels = 2;
  device_config.sampleRate = sample_rate;
  device_config.dataCallback = data_callback;
  device_config.pUserData = calloc(1, sizeof(rbncli_sample_buffer));

  if(ma_device_init(NULL, &device_config, device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    return -1;
  }

  if(ma_device_start(device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(device);
    return -1;
  }

  return 0;
}

void rbncli_send_tml_msg(rbn_instance* inst, tml_message* tml_msg) {
  rbn_msg msg;
  msg.channel = tml_msg->channel;
  msg.type = tml_msg->type;
  msg.key = tml_msg->key;
  msg.velocity = tml_msg->velocity;
  if(tml_msg->type == TML_PITCH_BEND) {
    // TML transforms pitch bend values as a 14 (in 16) bits integer but rbn expects actual MIDI bytes
    // so we put back values as two separate 7 (in 8) bits integers
    msg.u8[2] = tml_msg->pitch_bend & 0x7f;
    msg.u8[3] = tml_msg->pitch_bend >> 7;
  }
  rbncli_lock();
  rbn_send_msg(inst, msg);
  rbncli_unlock();
}

void rbncli_progress_bar(uint32_t current, uint32_t* last) {
  if(current == 100 && *last < 100) {
    printf("\rDone!\n");
    *last = current;
  } else if(!last || current > * last) {
    printf("\r%02u%%\t", current);
    fflush(stdout);
    if(last) {
      *last = current;
    }
  }
}
