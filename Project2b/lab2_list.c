#include "SortedList.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

void* threadFunc(void* input);
int hash(const char *key);
void mutexLock(pthread_mutex_t *lock);
void spinLock(int *lock);
void sigHandler();

SortedList_t *lists;
SortedListElement_t *elements;

int iterations, numLists;
int opt_yield = 0;
char *syncChar;
long threadTime;

int *sLocks;
pthread_mutex_t *locks;


int main(int argc, char **argv) {
  int opt, numThreads, i, j;
  char *numThreadsString, *iterationsString, *yieldString, *listsString;
  char testName[20] = "list";

  numThreads = 1;
  iterations = 1;
  numLists = 1;
  threadTime = 0;

  numThreadsString = NULL;
  iterationsString = NULL;
  yieldString = NULL;
  listsString = NULL;
  syncChar = NULL;

  struct option longOpts[] = {
    {"threads", required_argument, NULL, 0},
    {"iterations", required_argument, NULL, 1},
    {"yield", required_argument, NULL, 2},
    {"sync", required_argument, NULL, 3},
    {"lists", required_argument, NULL, 4},
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
    case 4:
      listsString = optarg;
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
      fprintf(stderr, "Invalid yield setting\n");
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

  if(listsString != NULL && (numLists = atoi(listsString)) <= 0) {
    fprintf(stderr, "Invalid ammount of lists specified\n");
    exit(1);
  }

  if((locks = malloc(numLists * sizeof(pthread_mutex_t))) == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    exit(1);
  }

  if((sLocks = malloc(numLists * sizeof(int))) == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    exit(1);
  }

  for(i = 0; i < numLists; i++) {
    sLocks[i] = 0;

    if(pthread_mutex_init((locks + i), NULL) != 0) {
      fprintf(stderr, "Error initializing mutex\n");
      exit(1);
    }       
  }

  if((lists = malloc(numLists * sizeof(SortedList_t))) == NULL) { 
    fprintf(stderr, "Error allocating memory\n");
    exit(1);
  }

  for(i = 0; i < numLists; i++) {
    lists[i].key = NULL;
    lists[i].next = &lists[i];
    lists[i].prev = &lists[i];
  }

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

  for(i = 0; i < numLists; i++) {
    if(SortedList_length(&lists[i]) != 0) {
      fprintf(stderr, "Inconsistency found in list\n");
      exit(2);
    }
  }

  free(inputs);
  free(threads);
  
  for(i = 0; i < numElements; i++) {
    free((void*)elements[i].key);
  }

  free(elements);
  free(sLocks);
  free(locks);
  free(lists);

  int operations = numThreads * iterations * 3;
  long runtime = (long)(endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
  long opAvg = runtime / operations;
  long lockTime = threadTime / operations;

  printf("%s,%d,%d,%d,%d,%ld,%ld,%ld\n", testName, numThreads, iterations, numLists, operations, runtime, opAvg, lockTime);

  return 0;
}

void* threadFunc(void* input) {
  int i,  bucket;
  int threadNum = *((int*)input);
  SortedListElement_t *deleteEl;
  SortedListElement_t *myElements = elements + (threadNum * iterations);

  if(syncChar == NULL) {
    for(i = 0; i < iterations; i++) {
      bucket = hash(myElements[i].key);
      SortedList_insert(&lists[bucket], (myElements + i));
    }
    
    for(i = 0; i < numLists; i++) {
      if(SortedList_length(&lists[i]) == -1) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
    }
    
    for(i = 0; i < iterations; i++) {
      bucket = hash(myElements[i].key);
      deleteEl = SortedList_lookup(&lists[bucket], myElements[i].key);
      
      if(deleteEl == NULL || (SortedList_delete(deleteEl) == 1)) {	  
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
    }
  } else if (!strcmp(syncChar, "m")) {
    for(i = 0; i < iterations; i++) {
      bucket = hash(myElements[i].key);
      mutexLock(&locks[bucket]);
      SortedList_insert(&lists[bucket], (myElements + i));
      pthread_mutex_unlock(&locks[bucket]);
    }
    
    for(i = 0; i < numLists; i++) {
      mutexLock(&locks[i]);
      if(SortedList_length(&lists[i]) == -1) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
      pthread_mutex_unlock(&locks[i]);
    }

    for(i = 0; i < iterations; i++) {
      bucket = hash(myElements[i].key);
      mutexLock(&locks[bucket]);
      deleteEl = SortedList_lookup(&lists[bucket], myElements[i].key);
      
      if(deleteEl == NULL || (SortedList_delete(deleteEl) == 1)) {	  
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
      
      pthread_mutex_unlock(&locks[bucket]);
    }
  } else if (!strcmp(syncChar, "s")) {
    for(i = 0; i < iterations; i++) {
      bucket = hash(myElements[i].key);
      spinLock(&sLocks[bucket]);
      SortedList_insert(&lists[bucket], (myElements + i));
      __sync_lock_release(&sLocks[bucket]);
    }

    for(i = 0; i < numLists; i++) {
      spinLock(&sLocks[i]);
      if(SortedList_length(&lists[i]) == -1) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
      __sync_lock_release(&sLocks[i]);
    }

    for(i = 0; i < iterations; i++) {
      bucket = hash(myElements[i].key);
      spinLock(&sLocks[bucket]);
      deleteEl = SortedList_lookup(&lists[bucket], myElements[i].key);
      
      if(deleteEl == NULL || (SortedList_delete(deleteEl) == 1)) {
	fprintf(stderr, "Inconsistency found in list\n");
	exit(2);
      }
      
      __sync_lock_release(&sLocks[bucket]);
    }
  }
  
  return NULL;
}

int hash(const char *key) {
  int bucket, i, keyLen;

  bucket = 0;
  keyLen = strlen(key);

  for(i = 0; i < keyLen; i++)
    bucket += key[i];

  bucket = bucket % numLists;
  return bucket;
}

void mutexLock(pthread_mutex_t *lock) {
  clockid_t clk = CLOCK_REALTIME;
  struct timespec startTime, endTime;

  if(clock_gettime(clk, &startTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }

  pthread_mutex_lock(lock);    
  
  if(clock_gettime(clk, &endTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }

  threadTime += (long)(endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
}

void spinLock(int *lock) {
  clockid_t clk = CLOCK_REALTIME;
  struct timespec startTime, endTime;

  if(clock_gettime(clk, &startTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }
  
  while(__sync_lock_test_and_set(lock, 1)) { }

  if(clock_gettime(clk, &endTime)) {
    fprintf(stderr, "Error obtaining clock time\n");
    exit(1);
  }

  threadTime += (long)(endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
}

void sigHandler() {
  fprintf(stderr, "Segmentation fault caught\n");
  exit(2);
}
