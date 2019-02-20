#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

void sighandler(int);

int main(int argc, char *argv[]) {
  int opt, ind, outd, segflag, catchflag;
  ind = 0; // input file descriptor (initialized to stdin)
  outd = 1; // output file descriptor (initialzied to stdout)
  segflag = 0; // segflag == false;
  catchflag = 0; // catchflag == false;

  char *buffer, *inputfile, *outputfile, *segpointer;
  buffer = (char*) malloc(sizeof(char));
  inputfile = NULL;
  outputfile = NULL;
  segpointer = NULL;
  
  struct option longopts[] = {
    {"input", required_argument, NULL, 0},
    {"output", required_argument, NULL, 1},
    {"segfault", no_argument, NULL, 2},
    {"catch", no_argument, NULL, 3},
    {NULL, 0, NULL, 0}
  };
 
  while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
    switch (opt) {
    case 0:
      inputfile = optarg;
      break;
    case 1:
      outputfile = optarg;
      break;
    case 2:
      segflag = 1;
      break;
    case 3:
      catchflag = 1;
      break;
    case '?':
      fprintf(stderr, "Usage: lab0 [--input file] [--output file] [--segfault] [--catch]\n");
      exit(1);
      break;
    }
  }
  
  if(inputfile != NULL && (ind = open(inputfile, O_RDONLY)) == -1) {
    fprintf(stderr, "Input file error (%s): %s\n", inputfile, strerror(errno));
    exit(2);
  }

  if(outputfile != NULL && (outd = creat(outputfile, S_IRWXU)) == -1) {
    fprintf(stderr, "Output file error (%s): %s\n", outputfile, strerror(errno));
    exit(3);
  } 

  if (catchflag) {
    signal(SIGSEGV, sighandler);
  }

  if (segflag) {
    *segpointer = 1;
  }

  while(read(ind, buffer, 1) > 0) {
    write(outd, buffer, 1);
  }

  close(ind);
  dup(outd);
  close(outd);
  return 0;
}

void sighandler(int signum) {
  if(signum == SIGSEGV) {
    fprintf(stderr, "Segmentation fault has been caught\n");
    exit(4);
  }
}
