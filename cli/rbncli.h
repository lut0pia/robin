#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "miniaudio.h"
struct tsf_stream;
#include "tml.h"
#include "../robin_general.h"

#define RBNCLI_SUCCESS 0
#define RBNCLI_ERR_EXIT -2
#define RBNCLI_ERR_UNKNOWN -1

static const uint32_t sample_rate = 44100;
extern rbn_instance inst;

int rbncli_play_mid(int argc, char** argv);
int rbncli_render_mid(int argc, char** argv);
int rbncli_open_device(int argc, char** argv);
int rbncli_edit_prg(int argc, char** argv);
int rbncli_export_prg(int argc, char** argv);
int rbncli_print_help(int argc, char** argv);

void rbncli_platform_init();
int rbncli_init_ma_device(ma_device* device);
void rbncli_send_tml_msg(rbn_instance* inst, tml_message* tml_msg);
uint64_t rbncli_get_time();
void rbncli_progress_bar(uint32_t current, uint32_t* last);
void rbncli_sleep(uint32_t ms);
void rbncli_lock();
void rbncli_unlock();
void rbncli_clear_screen();
int rbncli_getch();
