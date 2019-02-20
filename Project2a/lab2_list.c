#include "SortedList.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

void* threadFunc(void* input);
void sigHandler();

SortedList_t list;
SortedListElement_t *elements;

int iterations, yieldFlag, spinLock;
int opt_yield = 0;
char *syncChar;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) {
  int opt, numThreads, i, j;
  char *numThreadsString, *iterationsString, *yieldString;
  char testName[20] = "list";

  numThreads = 1;
  iterations = 1;
  spinLock = 0;

  numThreadsString = NULL;
  iterationsString = NULL;
  yieldString = NULL;
  syncChar = NULL;

  struct option longOpts[] = {
    {"threads", required_argument, NULL, 0},
    {"iterations", required_argument, NULL, 1},
    {"yield", required_argument, NULL, 2},
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
      yieldString = optarg;
      break;
    case 3:
      syncChar = optarg;
      break;
    default:
      fprintf(stderr, "Usage: lab2a [--threads int] [--iterations int] [--yield string] [--sync char]\n");
      exit(1);
    }
  }
  
  if(numThreadsString != NULL && (numThreads = atoi(numThreadsString)) <= 0) {
    fprintf(stderr, "Invalid ammount of threads specified\n");
    exit(1);
  }

  if(iterationsString != NULL && (iterations = atoi(iterationsString)) <= 0) {
    fprintf(stderr, "Invalid ammount of iterations specified\n");
    exit(1);
  }

  if(yieldString != NULL) {
    int errorFlag = 0;
    int len = strlen(yieldString);
    
    if(len > 3 || len == 0)
      errorFlag = 1;
    
    for(i = 0; i < len; i++) {
      switch(yieldString[i]) {
      case 'i':
	if(opt_yield & INSERT_YIELD)
	  errorFlag = 1;
	opt_yield |= INSERT_YIELD;
	break;
      case 'd':
	if(opt_yield & DELETE_YIELD)
	  errorFlag = 1;
	opt_yield |= DELETE_YIELD;
	break;
      case 'l':
	if(opt_yield & LOOKUP_YIELD)
	  errorFlag = 1;
	opt_yield |= LOOKUP_YIELD;
	break;
      default:
	errorFlag = 1;
      }
    }

    if(errorFlag) {
      fprintf(stderr, "Invalid yield setting");
      exit(1);
    }

    strcat(testName, "-");
    
    if(opt_yield & INSERT_YIELD)
      strcat(testName, "i");

    if(opt_yield & DELETE_YIELD)
      strcat(testName, "d");

    if(opt_yield & LOOKUP_YIELD)
      strcat(testName, "l");
  } else {
    strcat(testName, "-none"); 
  }

  if(syncChar != NULL) {
    if(!strcmp(syncChar, "m")) {
      strcat(testName, "-m");
    } else if (!strcmp(syncChar, "s")) {
      strcat(testName, "-s");
    } else {
      fprintf(stderr, "Invalid sync specified\n");
      exit(1);
    }
  } else {
    strcat(testName, "-none");
  }

  list.key = NULL;
  list.next = &list;
  list.prev = &list;

  int numElements = numThreads * iterations;

  srand(time(NULL));

  if((elements = malloc(numElements * sizeof(SortedListElement_t))) == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    exit(1);
  }

  int keyLen;
  char *key;

  for(i = 0; i < numElements; i++) {
    keyLen = (rand() % 10) + 1;
    
    if((key = malloc((keyLen + 1) * sizeof(char))) == NULL) {
      fprintf(stderr, "Error allocating memory\n");
      exit(1);
    }
    
    for(j = 0; j < keyLen; j++) {
      key[j] = (rand() % 58) + 65;
    }

    key[keyLen] = '\0';
    elements[i].key  = (const char *)key;
  }

  clockid_t clk = CLOCK_REALTIME;
  struct timespec startTime, endTime;

  if(clock_gettime(clk, &startTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }

  signal(SIGSEGV, sigHandler);

  pthread_t *threads;

  if((threads = (pthread_t*)malloc(numThreads * sizeof(pthread_t))) == NULL) {
    fprintf(stderr, "Error allocating memory for threads");
    exit(1);
  }
  
  int *inputs = malloc(numThreads * sizeof(int));

  for(i = 0; i < numThreads; i++) {
    inputs[i] = i;
    if(pthread_create(&threads[i], NULL, threadFunc, (void *)(inputs + i))) {
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

  if(clock_gettime(clk, &endTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }

  if(SortedList_length(&list) != 0) {
    fprintf(stderr, "Inconsistency found in list\n");
    exit(2);
  }

  free(inputs);
  free(threads);
  
  for(i = 0; i < numElements; i++) {
    free((void*)elements[i].key);
  }

  free(elements);
  
  int operations = numThreads * iterations * 3;
  long runtime = (long)(endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
  long opAvg = runtime / operations;

  printf("%s,%d,%d,1,%d,%ld,%ld\n", testName, numThreads, iterations, operations, runtime, opAvg);

  return 0;
}

void* threadFunc(void* input) {
  int i;
  int threadNum = *((int*)input);
  SortedListElement_t *deleteEl;
  SortedListElement_t *myElements = elements + (threadNum * iterations);

  if(syncChar == NULL) {
    for(i = 0; i < iterations; i++) {
      SortedList_insert(&list, (myElements + i));
    }
    
    if(SortedList_length(&list) == -1) {
      fprintf(stderr, "Inconsistency found in list\n");
      exit(2);
    }
    
    for(i = 0; i < iterations; i++) {
      deleteEl = SortedList_lookup(&list, myElements[i].key);
      if(SortedList_delete(deleteEl) == 1) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
    }
  } else if (!strcmp(syncChar, "m")) {
    for(i = 0; i < iterations; i++) {
      pthread_mutex_lock(&lock);
      SortedList_insert(&list, (myElements + i));
      pthread_mutex_unlock(&lock);
    }
    
    pthread_mutex_lock(&lock);
    if(SortedList_length(&list) == -1) {
      fprintf(stderr, "Inconsistency found in list\n");
      exit(2);
    }
    pthread_mutex_unlock(&lock);
    
    for(i = 0; i < iterations; i++) {
      pthread_mutex_lock(&lock);
      deleteEl = SortedList_lookup(&list, myElements[i].key);
      if(SortedList_delete(deleteEl) == 1) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
      pthread_mutex_unlock(&lock);
    }
  } else if (!strcmp(syncChar, "s")) {
    for(i = 0; i < iterations; i++) {
      while(__sync_lock_test_and_set(&spinLock, 1)) { }
      SortedList_insert(&list, (myElements + i));
      __sync_lock_release(&spinLock);
    }
    
    while(__sync_lock_test_and_set(&spinLock, 1)) { }
    if(SortedList_length(&list) == -1) {
      fprintf(stderr, "Inconsistency found in list\n");
      exit(2);
    }
    __sync_lock_release(&spinLock);
    
    for(i = 0; i < iterations; i++) {
      while(__sync_lock_test_and_set(&spinLock, 1)) { }
      deleteEl = SortedList_lookup(&list, myElements[i].key);
      if(SortedList_delete(deleteEl) == 1) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
      __sync_lock_release(&spinLock);
    }
  }

  return NULL;
}

void sigHandler() {
  fprintf(stderr, "Segmentation fault caught\n");
  exit(2);
}
