#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>

void add(long long *pointer, long long value);
void addMutex(long long *pointer, long long value);
void addSpin(long long *pointer, long long value);
void addCompare(long long *pointer, long long value);
void* addFunc(void* input);

int iterations, yieldFlag, spinLock;
char *syncChar;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) {
  int opt, numThreads;
  long long counter;
  char *numThreadsString, *iterationsString, *testName;
  
  counter = 0;
  numThreads = 1;
  iterations = 1;
  yieldFlag = 0;
  testName = "add-none";
  syncChar = NULL;
  spinLock = 0;

  struct option longOpts[] = {
    {"threads", required_argument, NULL, 0},
    {"iterations", required_argument, NULL, 1},
    {"yield", no_argument, NULL, 2},
    {"sync", required_argument, NULL, 3},
    {NULL, 0, NULL, 0}
  };

  while ((opt = getopt_long(argc, argv, "", longOpts, NULL)) != -1) {
    switch(opt) {
    case 0:
      numThreadsString = optarg;
      break;
    case 1:
      iterationsString = optarg;
      break;
    case 2:
      yieldFlag = 1;
      break;
    case 3:
      syncChar = optarg;
      break;
    default:
      fprintf(stderr, "Usage: lab2a [--threads int] [--iterations int] [--yield] [--sync char]\n");
      exit(1);
    }
  }
  
  if((numThreads = atoi(numThreadsString)) <= 0) {
    fprintf(stderr, "Invalid ammount of threads specified\n");
    exit(1);
  }

  if((iterations = atoi(iterationsString)) <= 0) {
    fprintf(stderr, "Invalid ammount of iterations specified\n");
    exit(1);
  }

  if(yieldFlag) {
    testName = "add-yield";
  }

  if(syncChar == NULL) {
  } else if(!strcmp(syncChar, "m")) {
    if(yieldFlag)
      testName = "add-yield-m";
    else
      testName = "add-m";
  } else if (!strcmp(syncChar, "s")) {
    if(yieldFlag)
      testName = "add-yield-s";
    else
      testName = "add-s";
  } else if (!strcmp(syncChar, "c")) {
    if(yieldFlag)
      testName = "add-yield-c";
    else
      testName = "add-c";
  } else {
    fprintf(stderr, "Invalid sync specified\n");
    exit(1);
  }

  clockid_t clk = CLOCK_REALTIME;
  struct timespec startTime, endTime;

  if(clock_gettime(clk, &startTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }

  int i;
  pthread_t *threads;

  if((threads = (pthread_t*)malloc(numThreads * sizeof(pthread_t))) == NULL) {
    fprintf(stderr, "Error allocating memory for threads");
    exit(1);
  }
  
  for(i = 0; i < numThreads; i++) {
    if(pthread_create(&threads[i], NULL, addFunc, (void *)&counter)) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }

  for(i = 0; i < numThreads; i++) {
    if(pthread_join(threads[i], NULL)) {
      fprintf(stderr, "Error joining threads\n");
      exit(1);
    }
  }

  free(threads);

  if(clock_gettime(clk, &endTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }
  
  int operations = numThreads * iterations * 2;
  long runtime = (long)(endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
  long opAvg = runtime / operations;

  printf("%s,%d,%d,%d,%ld,%ld,%lld\n", testName, numThreads, iterations, operations, runtime, opAvg, counter);

  return 0;
}

void add(long long *pointer, long long value) {
  long long sum = *pointer + value;

  if(yieldFlag)
    sched_yield();

  *pointer = sum;
}

void addMutex(long long *pointer, long long value) {
  pthread_mutex_lock(&lock);

  long long sum = *pointer + value;

  if(yieldFlag)
    sched_yield();

  *pointer = sum;

  pthread_mutex_unlock(&lock);
}

void addSpin(long long *pointer, long long value) {
  while(__sync_lock_test_and_set(&spinLock, 1) == 1) { }

  long long sum = *pointer + value;

  if(yieldFlag)
    sched_yield();

  *pointer = sum;

  __sync_lock_release(&spinLock);
}

void addCompare(long long *pointer, long long value) {
  long long oldVal, sum;

  do {
    oldVal = *pointer;
    sum = oldVal + value;
    if(yieldFlag)
      sched_yield();
  } while(__sync_val_compare_and_swap(pointer, oldVal, sum) != oldVal);
}

void* addFunc(void* input) {
  long long *counter = (long long*)input;
  int i;
  
  if (syncChar == NULL) {
    for(i = 0; i < iterations; i++) {
      add(counter, 1);
    }
    
    for(i = 0; i < iterations; i++) {
      add(counter, -1);
    }
  } else if (!strcmp(syncChar, "m")) {
    for(i = 0; i < iterations; i++) {
      addMutex(counter, 1);
    }
    
    for(i = 0; i < iterations; i++) {
      addMutex(counter, -1);
    }
  } else if (!strcmp(syncChar, "s")) {
    for(i = 0; i < iterations; i++) {
      addSpin(counter, 1);
    }
    
    for(i = 0; i < iterations; i++) {
      addSpin(counter, -1);
    }
  } else if (!strcmp(syncChar, "c")) {
    for(i = 0; i < iterations; i++) {
      addCompare(counter, 1);
    }
    
    for(i = 0; i < iterations; i++) {
      addCompare(counter, -1);
    }
  }

  return NULL;
}
