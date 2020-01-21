#include "rbncli.h"

static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 sample_count) {
  rbn_render(&inst, output, sample_count);
}

int rbncli_play_mid(const char* filename) {
  tml_message* mid_seq = tml_load_filename(filename);
  if(!mid_seq) {
    return -1;
  }

  unsigned int total_time;
  tml_get_info(mid_seq, NULL, NULL, NULL, NULL, &total_time);

  ma_device_config device_config;
  ma_device device;

  device_config = ma_device_config_init(ma_device_type_playback);
  device_config.playback.format = ma_format_s16;
  device_config.playback.channels = 2;
  device_config.sampleRate = sample_rate;
  device_config.dataCallback = data_callback;

  if(ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    tml_free(mid_seq);
    return -1;
  }

  if(ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    tml_free(mid_seq);
    ma_device_uninit(&device);
    return -1;
  }

  uint32_t progress = 0;
  rbncli_progress_bar(progress, NULL);

  tml_message* current_msg = mid_seq;
  uint32_t current_time = 0;
  while(current_msg) {
    rbncli_progress_bar((uint32_t)((current_time * 100) / total_time), &progress);

    int32_t time_to_wait = current_msg->time - current_time;
    if(time_to_wait > 0) {
      rbncli_sleep(time_to_wait);
      current_time += time_to_wait;
    }

    rbncli_send_tml_msg(&inst, current_msg);
    current_msg = current_msg->next;
  }

  rbncli_progress_bar(100, &progress);

  tml_free(mid_seq);
  ma_device_uninit(&device);

  return 0;
}
