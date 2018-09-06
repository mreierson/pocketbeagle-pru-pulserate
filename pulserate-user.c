#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include "pulserate_bin.h"

#define PRU_NUM 0
#define AM33XX

pthread_t thread;
pthread_mutex_t int_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t exit_condition = PTHREAD_COND_INITIALIZER;

static void *pru_mem;
static unsigned int *pru_mem_int;

void* rate_loop(void * arg)
{
  struct timeval now;
  struct timeval before;

  int curr_count = 0;
  int last_count = INT_MAX;
  int intv_count0 = 0;
  int intv_count1 = 0;
  int intv_count2 = 0;
  double elapsed_millis = 0;

  printf("last_count = %08X\n", last_count);
  
  int finished = 0;
  while (!finished) {

    gettimeofday(&now, NULL);
    
    elapsed_millis = (now.tv_sec - before.tv_sec) * 1000.0;
    elapsed_millis += (now.tv_usec - before.tv_usec) / 1000.0;
    before = now;

    curr_count = pru_mem_int[0];
    if (curr_count != 0xdeadbeef) {

      intv_count2 = intv_count1;
      intv_count1 = intv_count0;
      intv_count0 = (last_count - curr_count);

      double avg_count = (intv_count0 + intv_count1 + intv_count2) / 3.0f;        
      double hz = avg_count / (elapsed_millis / 1000.0);

      printf("current rate: last_count = %d, curr_count = %d (%08x), elapsed = %.3f, %.3f Hz\n", intv_count0, curr_count, curr_count, elapsed_millis, hz);
      last_count = curr_count;
    }

    struct timespec delay;
    timespec_get(&delay, TIME_UTC);    
    
    long ns = 250000000;
    int sec = ns / 1000000000;
    ns = ns - sec * 1000000000;

    delay.tv_nsec += ns;
    delay.tv_sec += delay.tv_nsec / 1000000000 + sec;
    delay.tv_nsec = delay.tv_nsec % 1000000000;
    
    pthread_mutex_lock(&mutex);
    pthread_cond_timedwait(&exit_condition, &mutex, &delay);
    pthread_mutex_unlock(&mutex);        
  }    
}

int main(int argc, char **argv)
{
  printf("Loading PRU core : %d byte(s)\n", sizeof(PRUcode));

  int rc;
  struct timeval before, after;
  tpruss_intc_initdata intc = PRUSS_INTC_INITDATA;
  
  if ((rc = prussdrv_init()) != 0) {
    printf("prussdrv_init failed\n");
    return rc;
  }
  
  if ((rc = prussdrv_open(PRU_EVTOUT_0)) != 0) {
    printf("prussdrv_open failed\n");
    return rc;
  }

  if ((rc = prussdrv_pruintc_init(&intc)) != 0) {
    printf("prussdrv_pruintc_init failed\n");
    return rc;
  }
  
  if ((rc = prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pru_mem)) != 0) {
    printf("prussdrv_map_prumem failed: rc = %d\n", rc);
    return rc;
  }

  pru_mem_int = (unsigned int*) pru_mem;
     
  if ((rc = prussdrv_exec_code(PRU_NUM, PRUcode, sizeof(PRUcode))) != 0) {
    printf("prussdrv_exec_code failed\n");
    return rc;
  }

  rc = prussdrv_pru_wait_event(PRU_EVTOUT_0);

  prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);

  pthread_create(&thread, NULL, rate_loop, NULL);
  
  pthread_mutex_lock(&mutex);
  pthread_cond_wait(&exit_condition, &mutex);
  pthread_mutex_unlock(&mutex);
  
  return 0;
}
