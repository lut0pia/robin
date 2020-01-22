#include "rbncli.h"

static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 sample_count) {
  rbn_render(&inst, output, sample_count);
}

int rbncli_init_ma_device(ma_device* device) {
  ma_device_config device_config;
  device_config = ma_device_config_init(ma_device_type_playback);
  device_config.playback.format = ma_format_s16;
  device_config.playback.channels = 2;
  device_config.sampleRate = sample_rate;
  device_config.dataCallback = data_callback;

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
  rbn_send_msg(inst, msg);
}

void rbncli_progress_bar(uint32_t current, uint32_t* last) {
  if(current == 100 && *last < 100) {
    printf("\rDone!\n");
    *last = current;
  } else if(!last || current > * last) {
    printf("\r%02u%%\t", current);
    if(last) {
      *last = current;
    }
  }
}
