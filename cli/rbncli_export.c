#include "rbncli.h"

#include <stdarg.h>

static void export_envelope(FILE* stream, const rbn_envelope* env, const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  for(uintptr_t i = 0; i < RBN_ENVPT_COUNT; i++) {
    if(env->points[i].time > 0.f || env->points[i].value > 0.f) {
      vfprintf(stream, fmt, va);
      fprintf(stream, ".points[%d].time = %f;\n", i, env->points[i].time);
      vfprintf(stream, fmt, va);
      fprintf(stream, ".points[%d].value = %f;\n", i, env->points[i].value);
    }
  }
  if(env->release_time != 0.f) {
    vfprintf(stream, fmt, va);
    fprintf(stream, ".release_time = %f;\n", env->release_time);
  }
  va_end(va);
}

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

  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    const rbn_operator* op = prg->operators + i;

    { // Avoid exporting operators that don't impact sound
      int has_impact = 0;

      if(op->output != 0.f) {
        has_impact = 1;
      }

      for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
        if(prg->op_matrix[i][j] != 0.f) {
          has_impact = 1;
        }
      }

      if(!has_impact) {
        continue;
      }
    }

    if(op->freq_ratio != 0.f) {
      fprintf(stream, "prg->operators[%d].freq_ratio = %f;\n", i, op->freq_ratio);
    }
    if(op->noise != 0.f) {
      fprintf(stream, "prg->operators[%d].noise = %f;\n", i, op->noise);
    }
    if(op->output != 0.f) {
      fprintf(stream, "prg->operators[%d].output = %f;\n", i, op->output);
    }
    export_envelope(stream, &op->volume_envelope, "prg->operators[%d].volume_envelope", i);
    export_envelope(stream, &op->pitch_envelope, "prg->operators[%d].pitch_envelope", i);

    for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
      if(prg->op_matrix[i][j] != 0.f) {
        fprintf(stream, "prg->op_matrix[%d][%d] = %f;\n", i, j, prg->op_matrix[i][j]);
      }
    }
  }

  if(stream != stdout) {
    fclose(stream);
  }

  return 0;
}
