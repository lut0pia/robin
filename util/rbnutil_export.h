#include <stdarg.h>
#include <string.h>

#include "../robin.h"

static void export_float(FILE* stream, float value) {
  char buffer[32];
  sprintf(buffer, "%f", value);
  uintptr_t len = strlen(buffer);
  while(buffer[len - 1] == '0') {
    buffer[--len] = '\0';
  }
  fprintf(stream, "%sf", buffer);
}

static void export_envelope(FILE* stream, const rbn_envelope* env, const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  for(uintptr_t i = 0; i < RBN_ENVPT_COUNT; i++) {
    if(env->points[i].time > 0.f || env->points[i].value > 0.f) {
      vfprintf(stream, fmt, va);
      fprintf(stream, ".points[%llu].time = ", i);
      export_float(stream, env->points[i].time);
      fprintf(stream, ";\n");

      vfprintf(stream, fmt, va);
      fprintf(stream, ".points[%llu].value = ", i);
      export_float(stream, env->points[i].value);
      fprintf(stream, ";\n");
    }
  }
  if(env->release_time != 0.f) {
    vfprintf(stream, fmt, va);
    fprintf(stream, ".release_time = ");
    export_float(stream, env->release_time);
    fprintf(stream, ";\n");
  }
  va_end(va);
}

void rbnutil_export(FILE* stream, const rbn_program* prg) {
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
      fprintf(stream, "prg->operators[%llu].freq_ratio = ", i);
      export_float(stream, op->freq_ratio);
      fprintf(stream, ";\n");
    }
    if(op->noise != 0.f) {
      fprintf(stream, "prg->operators[%llu].noise = ", i);
      export_float(stream, op->noise);
      fprintf(stream, ";\n");
    }
    if(op->output != 0.f) {
      fprintf(stream, "prg->operators[%llu].output = ", i);
      export_float(stream, op->output);
      fprintf(stream, ";\n");
    }
    export_envelope(stream, &op->volume_envelope, "prg->operators[%d].volume_envelope", i);
    export_envelope(stream, &op->pitch_envelope, "prg->operators[%d].pitch_envelope", i);

    for(uintptr_t j = 0; j < RBN_OPERATOR_COUNT; j++) {
      if(prg->op_matrix[i][j] != 0.f) {
        fprintf(stream, "prg->op_matrix[%llu][%llu] = ", i, j);
        export_float(stream, prg->op_matrix[i][j]);
        fprintf(stream, ";\n");
      }
    }
  }
}
