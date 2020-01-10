#include <stdlib.h>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#include "miniaudio.h"

#define TML_IMPLEMENTATION
#define TML_ERROR(str) printf("%s\n", str);
#include "tml.h"

#define RBN_IMPLEMENTATION
#include "../robin.h"

static const uint32_t sample_rate = 44100;
static rbn_instance inst = {0};
#ifdef _WIN32
HMIDIIN hmi = NULL;
#endif

static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 sample_count) {
  rbn_render(&inst, output, sample_count);
}

#if _WIN32
static void CALLBACK event_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
  printf("%d\n", wMsg);
}
#endif

#if _WIN32
double perfcounter_mult;
uint64_t get_time() {
  int64_t counter;
  QueryPerformanceCounter((LARGE_INTEGER*)&counter);
  return (uint64_t)(counter * perfcounter_mult);
}
#else
#endif

static void rbn_send_tml_msg(rbn_instance* inst, tml_message* tml_msg) {
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

static void fputui(unsigned int value, size_t size, FILE* stream) {
  while(size > 0) {
    fputc(value & 0xff, stream);
    size -= 1;
    value >>= 8;
  }
}

static void progress_bar(uint32_t current, uint32_t* last) {
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

static int play_mid(const char* filename) {
  tml_message* mid_seq = tml_load_filename(filename);
  if(!mid_seq) {
    tml_free(mid_seq);
    rbn_shutdown(&inst);
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
    rbn_shutdown(&inst);
    return -1;
  }

  if(ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    tml_free(mid_seq);
    rbn_shutdown(&inst);
    ma_device_uninit(&device);
    return -1;
  }

  uint32_t progress = 0;
  progress_bar(progress, NULL);

  tml_message* current_msg = mid_seq;
  uint32_t current_time = 0;
  while(current_msg) {
    progress_bar((uint32_t)((current_time * 100) / total_time), &progress);

    int32_t time_to_wait = current_msg->time - current_time;
    if(time_to_wait > 0) {
      Sleep(time_to_wait);
      current_time += time_to_wait;
    }

    rbn_send_tml_msg(&inst, current_msg);
    current_msg = current_msg->next;
  }

  progress_bar(100, &progress);

  tml_free(mid_seq);
  ma_device_uninit(&device);

  return 0;
}

static int render_mid(const char* filename) {
  tml_message* mid_seq = tml_load_filename(filename);
  if(!mid_seq) {
    rbn_shutdown(&inst);
    return -1;
  }

  unsigned int total_time;
  tml_get_info(mid_seq, NULL, NULL, NULL, NULL, &total_time);

  char wavfilename[128] = "";
  sprintf(wavfilename, "%s.wav", filename);

  const uint32_t channels = 2;
  const uint32_t bytes_per_block = sizeof(int16_t) * channels;
  const uint32_t bits_per_sample = sizeof(int16_t) * 8;
  const uint32_t bytes_per_second = (sample_rate * bits_per_sample * channels) / 8;

  FILE* wavfile = fopen(wavfilename, "wb");
  fputs("RIFF----WAVEfmt ", wavfile);
  fputui(16, 4, wavfile); // No extension data
  fputui(1, 2, wavfile); // PCM
  fputui(channels, 2, wavfile); // Channels
  fputui(sample_rate, 4, wavfile); // Sample rate
  fputui(bytes_per_second, 4, wavfile); // Byte rate
  fputui(bytes_per_block, 2, wavfile); // Bytes per block
  fputui(bits_per_sample, 2, wavfile); // Bits per sample

  const long data_chunk_pos = ftell(wavfile);
  fputs("data----", wavfile);

  uint32_t progress = 0;
  progress_bar(progress, NULL);

  tml_message* current_msg = mid_seq;
  uint64_t current_sample = 0;
  uint64_t total_rendering_time = 0;
  uint64_t total_rendered_samples = 0;
  while(current_msg) {
    const uint64_t current_time = (current_sample * 1000) / sample_rate;

    progress_bar((uint32_t)((current_time * 100) / total_time), &progress);

    const int64_t time_to_wait = current_msg->time - current_time;
    if(time_to_wait > 0) {
      uint32_t samples_to_render = (uint32_t)((time_to_wait * sample_rate) / 1000);

      // Pad sample count to block size
      if(samples_to_render % RBN_BLOCK_SAMPLES > 0) {
        samples_to_render += RBN_BLOCK_SAMPLES - (samples_to_render % RBN_BLOCK_SAMPLES);
      }

      int16_t* buffer = malloc(samples_to_render * sizeof(int16_t) * 2);

      const uint64_t previous_rendered_samples = inst.rendered_samples;
      const uint64_t previous_time = get_time();

      rbn_result result = rbn_render(&inst, buffer, samples_to_render);
      if(result != rbn_success) {
        printf("rbn_render failed\n");
        tml_free(mid_seq);
        fclose(wavfile);
        return -1;
      }

      total_rendering_time += get_time() - previous_time;
      total_rendered_samples += inst.rendered_samples - previous_rendered_samples;

      fwrite(buffer, sizeof(int16_t) * 2, samples_to_render, wavfile);

      free(buffer);

      current_sample += samples_to_render;
    }

    rbn_send_tml_msg(&inst, current_msg);
    current_msg = current_msg->next;
  }

  progress_bar(100, &progress);

  const long file_size = ftell(wavfile);

  // Fix the data chunk header to contain the data size
  fseek(wavfile, data_chunk_pos + 4, SEEK_SET);
  fputui(file_size - data_chunk_pos + 8, 4, wavfile);

  // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
  fseek(wavfile, 4, SEEK_SET);
  fputui(file_size - 8, 4, wavfile);

  tml_free(mid_seq);
  fclose(wavfile);

  printf("Samples per us: %f\n", (double)total_rendered_samples / (double)total_rendering_time);

  return 0;
}

static int open_device(int argc, char** argv) {
  if(argc == 0) { // List available devices
#ifdef _WIN32
    UINT nMidiDeviceNum;
    nMidiDeviceNum = midiInGetNumDevs();
    if(nMidiDeviceNum == 0) {
      printf("No MIDI device detected\n");
      return -1;
    }

    printf("Devices:\n");
    MIDIINCAPS caps;
    for(UINT i = 0; i < nMidiDeviceNum; ++i) {
      midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));
      printf("- %d: name = %s\n", i, caps.szPname);
    }
#endif
  } else {
#ifdef _WIN32
    UINT device_id = atoi(argv[0]);
    MMRESULT result = midiInOpen(&hmi, device_id, (DWORD_PTR)event_callback, (DWORD_PTR)NULL, CALLBACK_FUNCTION);
    if(result != MMSYSERR_NOERROR) {
      printf("Could not open MIDI device %d: %x\n", device_id, result);
      return -1;
    }
    midiInStart(hmi);

#endif
  }
  return 0;
}

static int print_help(int argc, char** argv) {
  printf(
    "rbncli v0.1\n"
    "- play [file.mid]\n"
    "- render [file.mid]\n"
    "- open [device_id]\n"
    "- exit\n"
  );
  return 0;
}

static int handle_cmd(int argc, char** argv) {
  if(argc >= 2 && !strcmp(argv[0], "play")) {
    return play_mid(argv[1]);
  } else if(argc >= 2 && !strcmp(argv[0], "render")) {
    return render_mid(argv[1]);
  } else if(argc >= 1 && !strcmp(argv[0], "open")) {
    return open_device(argc - 1, argv + 1);
  } else {
    print_help(argc, argv);
    return -1;
  }
}

int main(int argc, char** argv) {
#if _WIN32
  // Initialize multiplier for performance counter on Windows
  LARGE_INTEGER freq;
  QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
  perfcounter_mult = 1.0 / ((double)freq.QuadPart / 1000000.0);
#endif

  rbn_config config = {
    .sample_rate = sample_rate,
    .sample_format = rbn_s16,
  };
  rbn_init(&inst, &config);

  if(argc > 1) {
    return handle_cmd(argc - 1, argv + 1);
  }

  // Enter interactive mode

  char cmd[512] = "";
  char* argv2[16] = {NULL};
  int argc2;
  do {
    fputs("> ", stdout);
    fgets(cmd, sizeof(cmd), stdin);

    argc2 = 0;
    do {
      argv2[argc2] = strtok(argc2 == 0 ? cmd : NULL, " \n");
    } while(argv2[argc2++]);
    argc2--;

    handle_cmd(argc2, argv2);

  } while(argc2 < 1 || strcmp(argv2[0], "exit"));

  return 0;
}
