#include "rbncli.h"

#include <pthread.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void rbncli_platform_init() {
  pthread_mutex_init(&mutex, NULL);
}

uint64_t rbncli_get_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_usec + (uint64_t)tv.tv_sec * 1000000;
}

void rbncli_sleep(uint32_t ms) {
  usleep(ms * 1000);
}

void rbncli_lock() {
  pthread_mutex_lock(&mutex);
}

void rbncli_unlock() {
  pthread_mutex_unlock(&mutex);
}

void rbncli_clear_screen() {
  system("clear");
}

int rbncli_getch() {
  struct termios old, current;
  tcgetattr(0, &old); /* grab old terminal i/o settings */
  current = old; /* make new settings same as old settings */
  current.c_lflag &= ~ICANON; /* disable buffered i/o */
  current.c_lflag &= ~ECHO; /* set no echo mode */
  tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
  int ch = getchar();
  tcsetattr(0, TCSANOW, &old);
  return ch;
}
