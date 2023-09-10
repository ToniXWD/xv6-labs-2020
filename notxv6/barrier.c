#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //

  // 先获取锁
  pthread_mutex_lock(&bstate.barrier_mutex);
  
  while (bstate.nthread == nthread && round != 0) {
    // bstate.nthread == nthread 且 round != 0 表示：上一个epoch还有线程没有退出
    // 需要等待线程一起进入本次epoch
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }

  // 进入本次epoch的第一个线程需要将 nthread 重置
  if (bstate.nthread == nthread) {
    bstate.nthread = 0;
  }

  // round记录了正在使用bstate.nthread的线程数
  round++;

  int cur_round = bstate.round;
  // 将达到barrier的线程数自增

  bstate.nthread++;
  while (bstate.nthread != nthread) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }

  // 运行到此处时都已经到达了线程屏障
  bstate.round = cur_round + 1;
  round--;

  // 唤醒线程
  pthread_cond_broadcast(&bstate.barrier_cond);
  // 释放锁
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
    // printf("epoch %i complete\n",i);
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
