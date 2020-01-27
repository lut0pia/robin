#include "rbncli.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#include "miniaudio.h"

#define TML_IMPLEMENTATION
#define TML_ERROR(str) printf("%s\n", str);
#include "tml.h"

#define RBN_IMPLEMENTATION
#include "../robin.h"

rbn_instance inst = {0};

int rbncli_print_help(int argc, char** argv) {
  printf(
    "rbncli v0.1\n"
    "- play [file.mid]\n"
    "- render [file.mid|demo]\n"
    "- open [device_id]\n"
    "- edit [prg_id]\n"
    "- export [prg_id]\n"
    "- exit\n"
  );
  return 0;
}

static int handle_cmd(int argc, char** argv) {
  if(argc >= 2 && !strcmp(argv[0], "play")) {
    return rbncli_play_mid(argc - 1, argv + 1);
  } else if(argc >= 2 && !strcmp(argv[0], "render")) {
    return rbncli_render_mid(argc - 1, argv + 1);
  } else if(argc >= 1 && !strcmp(argv[0], "open")) {
    return rbncli_open_device(argc - 1, argv + 1);
  } else if(argc >= 1 && !strcmp(argv[0], "edit")) {
    return rbncli_edit_prg(argc - 1, argv + 1);
  } else if(argc >= 1 && !strcmp(argv[0], "export")) {
    return rbncli_export_prg(argc - 1, argv + 1);
  } else {
    rbncli_print_help(argc, argv);
    return -1;
  }
}

int main(int argc, char** argv) {
  rbncli_platform_init();

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
