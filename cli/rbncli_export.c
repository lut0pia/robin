#include "rbncli.h"

#include "../util/rbnutil_export.h"

int rbncli_export_prg(int argc, char** argv) {
  if(argc == 0) {
    rbncli_print_help(0, NULL);
    return -1;
  }

  const uintptr_t prg_index = atoi(argv[0]);
  const rbn_program* prg = inst.programs + prg_index;

  FILE* stream = stdout;
  if(argc > 1) {
    stream = fopen(argv[1], "w");
    if(!stream) {
      printf("Couldn't write to file '%s'\n", argv[1]);
      return -1;
    }
  }

  rbnutil_export(stream, prg);

  if(stream != stdout) {
    fclose(stream);
  }

  return 0;
}
