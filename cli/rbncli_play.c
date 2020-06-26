#include "rbncli.h"

int rbncli_play_mid(int argc, char** argv) {
  const char* filename = argv[0];
  const uint32_t channel_mask = argc > 1 ? (1 << atoi(argv[1])) : ~0;

  tml_message* mid_seq = tml_load_filename(filename);
  if(!mid_seq) {
    return -1;
  }

  unsigned int total_time;
  tml_get_info(mid_seq, NULL, NULL, NULL, NULL, &total_time);

  ma_device device;
  if(rbncli_init_ma_device(&device) != MA_SUCCESS) {
    tml_free(mid_seq);
    return -1;
  }

  uint32_t progress = 0;
  rbncli_progress_bar(progress, NULL);

  rbn_reset(&inst);

  tml_message* current_msg = mid_seq;
  uint32_t current_time = 0;
  while(current_msg) {
    rbncli_progress_bar((uint32_t)((current_time * 100) / total_time), &progress);

    int32_t time_to_wait = current_msg->time - current_time;
    if(time_to_wait > 0) {
      rbncli_sleep(time_to_wait);
      current_time += time_to_wait;
    }

    if((1 << current_msg->channel) & channel_mask) {
      rbncli_send_tml_msg(&inst, current_msg);
    }
    current_msg = current_msg->next;
  }

  rbncli_progress_bar(100, &progress);

  tml_free(mid_seq);
  ma_device_uninit(&device);

  return 0;
}
