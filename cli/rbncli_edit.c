#include "rbncli.h"

#define RBNCLI_KEY_BACKSPACE 8
#define RBNCLI_KEY_CTRL_RETURN 10
#define RBNCLI_KEY_RETURN 13
#define RBNCLI_KEY_ESCAPE 27

#define RBNCLI_KEY_UP 72
#define RBNCLI_KEY_LEFT 75
#define RBNCLI_KEY_RIGHT 77
#define RBNCLI_KEY_DOWN 80

#define RBNCLI_KEY_CTRL_UP 141
#define RBNCLI_KEY_CTRL_LEFT 115
#define RBNCLI_KEY_CTRL_RIGHT 116
#define RBNCLI_KEY_CTRL_DOWN 145

typedef enum rbncli_edit_state {
  rces_prg,
  rces_op,
  rces_op_offset,
  rces_op_ratio,
  rces_op_noise,
  rces_op_output,
  rces_op_modulation,
  rces_op_env,
  rces_op_env_release,
  rces_op_env_point,
  rces_max,
} rbncli_edit_state;

static rbncli_edit_state back_state[rces_max] = {0};

static void print_op_mod(rbn_program* prg, uintptr_t op_index) {
  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    printf("%4.1f ", prg->op_matrix[op_index][i]);
  }
}

static void print_matrix(rbn_program* prg) {
  for(uintptr_t i = 0; i < RBN_OPERATOR_COUNT; i++) {
    printf("Op: %d\t", i);
    print_op_mod(prg, i);
    printf("Out:%.2f\n", prg->operators[i].output);
  }
}

static float get_env_value(rbn_envelope* env, float time) {
  float sustain_time;
  float sustain_value;
  for(uintptr_t i = 0; i < RBN_ENVPT_COUNT; i++) {
    float next_time = env->points[i].time;
    if(time == next_time) {
      return env->points[i].value;
    } else if(time < next_time) {
      float previous_time = i == 0 ? 0.f : env->points[i - 1].time;
      float previous_value = i == 0 ? 0.f : env->points[i - 1].value;
      float alpha = (time - previous_time) / (next_time - previous_time);
      return previous_value * (1.f - alpha) + env->points[i].value * alpha;
    } else if(i != 0 && next_time == 0.f) {
      sustain_time = env->points[i - 1].time;
      sustain_value = env->points[i - 1].value;
      break;
    }
  }

  float since_release = time - sustain_time;
  return sustain_value * (1.f - since_release / env->release_time);
}

static void print_envelope(rbn_envelope* env) {
  float sustain_time = 0.f;
  float max_value = 0.f;
  float min_value = 0.f;
  for(uintptr_t i = 0; i < RBN_ENVPT_COUNT; i++) {
    if(env->points[i].time > sustain_time) {
      sustain_time = env->points[i].time;
    }
    if(env->points[i].value > max_value) {
      max_value = env->points[i].value;
    }
    if(env->points[i].value < min_value) {
      min_value = env->points[i].value;
    }
  }

  const float total_time = sustain_time + (env->release_time > 0.f ? env->release_time : 0.f);
  const float value_range = max_value - min_value;

  if(value_range == 0.f) {
    printf("0\n");
    return;
  }

  const int cols = 64;
  const int rows = 10;
  const float row_value_step = value_range / rows;

  for(int row = 0; row < rows; row++) {
    const float row_value = max_value - value_range * ((float)row / rows);
    printf("%5.2f ", row_value);
    for(int col = 0; col < cols; col++) {
      const float value = get_env_value(env, ((float)col / cols) * total_time);
      if(row_value <= value) {
        putchar('|');
      } else if(row_value <= value + row_value_step * 0.15f) {
        putchar('0');
      } else if(row_value <= value + row_value_step * 0.3f) {
        putchar('o');
      } else if(row_value <= value + row_value_step * 0.45f) {
        putchar('-');
      } else if(row_value <= value + row_value_step * 0.6f) {
        putchar('.');
      } else if(row_value <= value + row_value_step * 0.75f) {
        putchar('_');
      } else {
        putchar(' ');
      }
    }
    printf("\n");
  }

  {
    printf("     ");
    for(int col = 0; col < cols; col += 5) {
      const float time = ((float)col / cols) * total_time;
      printf("%.2f ", time);
    }
    printf("\n");
  }
}

int rbncli_edit_prg(int argc, char** argv) {
  if(argc == 0) {
    rbncli_print_help(0, NULL);
    return -1;
  }

  back_state[rces_op_offset] = rces_op;
  back_state[rces_op_ratio] = rces_op;
  back_state[rces_op_noise] = rces_op;
  back_state[rces_op_output] = rces_op;
  back_state[rces_op_modulation] = rces_op;
  back_state[rces_op_modulation] = rces_op;
  back_state[rces_op_env] = rces_op;
  back_state[rces_op_env_release] = rces_op_env;
  back_state[rces_op_env_point] = rces_op_env;

  ma_device device;
  if(rbncli_init_ma_device(&device) != MA_SUCCESS) {
    return -1;
  }

  const uintptr_t prg_index = atoi(argv[0]);
  uintptr_t op_index = 0;
  uintptr_t op_mod_index = 0;

  rbn_envelope* env = NULL;
  uintptr_t env_point_index = 0;
  const char* env_name = "";

  int c = 0;
  int playing_note = 0;
  rbncli_edit_state state = rces_prg;

  rbn_set_program(&inst, 0, prg_index);

  do {
    rbn_program* prg = inst.programs + prg_index;
    rbn_operator* op = prg->operators + op_index;
    float* value = NULL;
    float* secondary_value = NULL;

    rbncli_clear_screen();
    printf("%d\n", c);
    // Header
    printf("Program: %d\n", prg_index);

    // Matrix
    switch(state) {
      case rces_prg:
        print_matrix(prg);
        break;
      case rces_op:
        printf("  Operator: %d\n", op_index);
        printf("    Offset: %f\n", op->freq_offset);
        printf("    Ratio:  %f\n", op->freq_ratio);
        printf("    Noise:  %f\n", op->noise);
        printf("    Output: %f\n", op->output);
        printf("    Mod:    ");
        print_op_mod(prg, op_index);
        printf("\n");
        printf("    Volume envelope:\n");
        print_envelope(&prg->operators[op_index].volume_envelope);
        printf("\n");
        printf("    Pitch envelope:\n");
        print_envelope(&prg->operators[op_index].pitch_envelope);
        break;
      case rces_op_ratio:
        printf("  Operator: %d\n", op_index);
        printf("    Ratio:  %f\n", op->freq_ratio);
        break;
      case rces_op_noise:
        printf("  Operator: %d\n", op_index);
        printf("    Noise:  %f\n", op->noise);
        break;
      case rces_op_output:
        printf("  Operator: %d\n", op_index);
        printf("    Output: %f\n", op->output);
        break;
      case rces_op_modulation:
        printf("  Operator: %d\n", op_index);
        printf("  to operator: %d\n", op_mod_index);
        printf("    Modulation: %f\n", prg->op_matrix[op_index][op_mod_index]);
        break;
      case rces_op_env:
        printf("  Operator: %d\n", op_index);
        printf("    %s envelope:\n", env_name);
        print_envelope(env);
        break;
      case rces_op_env_release:
        printf("  Operator: %d\n", op_index);
        printf("    %s envelope:\n", env_name);
        print_envelope(env);
        printf("Release: %f\n", env->release_time);
        break;
      case rces_op_env_point:
        printf("  Operator: %d\n", op_index);
        printf("    %s envelope:\n", env_name);
        print_envelope(env);
        printf("Point %d: (%f;%f)\n", env_point_index, env->points[env_point_index].time, env->points[env_point_index].value);
        break;
      default: break;
    }

    // Interaction
    c = rbncli_getch();
    if(c == RBNCLI_KEY_BACKSPACE) {
      state = back_state[state];
    }
    switch(state) {
      case rces_prg:
        if(c >= '0' && c <= '9' && c < '0' + RBN_OPERATOR_COUNT) {
          op_index = c - '0';
          state = rces_op;
        }
        break;
      case rces_op:
        if(c >= '0' && c <= '9' && c < '0' + RBN_OPERATOR_COUNT) {
          op_mod_index = c - '0';
          state = rces_op_modulation;
        }
        switch(c) {
          case 'o':
            state = rces_op_output;
            break;
          case 'r':
            state = rces_op_ratio;
            break;
          case 'n':
            state = rces_op_noise;
            break;
          case 'v':
            env_name = "Volume";
            env = &prg->operators[op_index].volume_envelope;
            state = rces_op_env;
            break;
          case 'p':
            env_name = "Pitch";
            env = &prg->operators[op_index].pitch_envelope;
            state = rces_op_env;
            break;
          default: break;
        }
        break;
      case rces_op_ratio:
        value = &op->freq_ratio;
        break;
      case rces_op_noise:
        value = &op->noise;
        break;
      case rces_op_output:
        value = &op->output;
        break;
      case rces_op_modulation:
        value = &prg->op_matrix[op_index][op_mod_index];
        break;
      case rces_op_env:
        if(c >= '0' && c <= '9' && c < '0' + RBN_ENVPT_COUNT) {
          env_point_index = c - '0';
          state = rces_op_env_point;
        } else if(c == 'r') {
          state = rces_op_env_release;
        }
        break;
      case rces_op_env_release:
        value = &env->release_time;
        break;
      case rces_op_env_point:
        value = &env->points[env_point_index].value;
        secondary_value = &env->points[env_point_index].time;
        break;
      default: break;
    }

    if(c == ' ') {
      if(playing_note) {
        rbn_stop_note(&inst, 0, 60);
      } else {
        rbn_play_note(&inst, 0, 60, 127);
      }
      playing_note = !playing_note;
    }

    if(value != NULL) {
      switch(c) {
        case RBNCLI_KEY_UP:
          *value += 0.1f;
          break;
        case RBNCLI_KEY_DOWN:
          *value -= 0.1f;
          break;
        case RBNCLI_KEY_CTRL_UP:
          *value += 0.01f;
          break;
        case RBNCLI_KEY_CTRL_DOWN:
          *value -= 0.01f;
          break;
        case RBNCLI_KEY_RETURN:
          printf("> ");
          scanf("%f", value);
          break;
        default: break;
      }

      rbn_refresh(&inst);
    }

    if(secondary_value != NULL) {
      const float prev_value = *secondary_value;
      switch(c) {
        case RBNCLI_KEY_RIGHT:
          *secondary_value += 0.1f;
          break;
        case RBNCLI_KEY_LEFT:
          *secondary_value -= 0.1f;
          break;
        case RBNCLI_KEY_CTRL_RIGHT:
          *secondary_value += 0.01f;
          break;
        case RBNCLI_KEY_CTRL_LEFT:
          *secondary_value -= 0.01f;
          break;
        case RBNCLI_KEY_CTRL_RETURN:
          printf("> ");
          scanf("%f", secondary_value);
          break;
        default: break;
      }

      if(state = rces_op_env_point) {
        for(uintptr_t i = env_point_index + 1; i < RBN_ENVPT_COUNT; i++) {
          if(env->points[i].time > 0.f) {
            env->points[i].time += *secondary_value - prev_value;
          }
        }
      }

      rbn_refresh(&inst);
    }
  } while(c != 27);

  rbncli_clear_screen();
  rbn_stop_all_notes(&inst);

  ma_device_uninit(&device);

  return 0;
}
