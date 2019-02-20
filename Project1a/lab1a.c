#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <poll.h>

void setOrig(struct termios term);
void readWrite(int fdIn, int fdOut);
void sigHandler(int sigNum);

struct termios term;

int main(int argc, char** argv) {
  struct termios termCopy;

  if (tcgetattr(0, &term) == -1) {
    fprintf(stderr, "Failed to get terminal parameters");
    exit(1);
  }

  termCopy = term;
  termCopy.c_iflag = ISTRIP;
  termCopy.c_oflag = 0;
  termCopy.c_lflag = 0;

  if (tcsetattr(0, TCSANOW, &termCopy) == -1) {
    fprintf(stderr, "Failed to set terminal to new parameters");
    exit(1);
  }
  
  int opt;
  char *inputShell;
  int inputFlag = 0;

  struct option longOpts[] = {
    {"shell", required_argument, NULL, 0},
    {NULL, 0, NULL, 0}
  };

  while ((opt = getopt_long(argc, argv, "", longOpts, NULL)) != -1) {
    switch (opt) {
    case 0:
      inputFlag = 1;
      inputShell = optarg;
      break;
    case '?':
      fprintf(stderr, "Usage: lab1a [--shell file]\n");
      setOrig(term);      
      exit(1);
    }
  }

  if(!inputFlag) {
    readWrite(0, 1);
    setOrig(term);
    return 0;
  }

  int pipeTS[2], pipeFS[2];

  if(pipe(pipeTS) == -1 || pipe(pipeFS) == -1) {
    fprintf(stderr, "Failed to create pipes");
    setOrig(term);
    exit(1);
  }

  pid_t id;

  if((id = fork()) == -1) {
    fprintf(stderr, "Failed to create child process");
    setOrig(term);
    exit(1);
  }

  if(id == 0) {
    dup2(pipeTS[0], 0);
    dup2(pipeFS[1], 1);
    dup2(pipeFS[1], 2);
    
    if(close(pipeTS[0]) == -1 || close(pipeTS[1]) == -1 || 
       close(pipeFS[0]) == -1 || close(pipeFS[1]) == -1) {
      fprintf(stderr, "Failed to close pipes");
      setOrig(term);
      exit(1);
    }

    if(execl(inputShell, "sh", (char *) NULL) == -1) {
      fprintf(stderr, "Failed to execute shell");
      setOrig(term);
      exit(1);
    }
  } else {
    if(signal(SIGPIPE, sigHandler) == SIG_ERR) {
      fprintf(stderr, "Failed to set up signal handler");
      setOrig(term);
      exit(1);
    }

    if(close(pipeTS[0]) == -1 || close(pipeFS[1]) == -1) {
      fprintf(stderr, "Failed to close pipes");
      setOrig(term);
      exit(1);
    }

    struct pollfd fds[] = {
      {0, POLLIN, 0},
      {pipeFS[0], POLLIN, 0}
    };

    const int bufferSize = 1024;
    int count, i;
    char buffer[bufferSize];
    char crlfBuffer[2];
    crlfBuffer[0] = 13;
    crlfBuffer[1] = 10;

    while(1) {
      if(poll(fds, 2, -1) == -1) {
	fprintf(stderr, "Failure to poll for data");
	setOrig(term);
	exit(1);
      }

      if(fds[0].revents == POLLERR || fds[0].revents == POLLHUP || fds[0].revents == POLLNVAL) {
	fprintf(stderr, "Failure to poll for keyboard input");
       	setOrig(term);
       	exit(1);
      }

      if(fds[1].revents == POLLERR || fds[1].revents == POLLNVAL) {
	fprintf(stderr, "Failure to poll for shell output");
       	setOrig(term);
	exit(1);
      }

      if(fds[0].revents & POLLIN) {
	count = read(0, buffer, bufferSize);
	for(i = 0; i < count; i++) {
	  if(buffer[i] == 3) {
	    if(kill(id, SIGINT) == -1) {
	      fprintf(stderr, "Failed to kill child process");
	      setOrig(term);
	      exit(1);
	    } 
	  } else if(buffer[i] == 4) {
	    if(close(pipeTS[1]) == -1) {
	      fprintf(stderr, "Failed to close pipes");
	      setOrig(term);
	      exit(1);
	    }
	    break;
	  } else if(buffer[i] == 13 || buffer[i] == 10) {
	    write(1, crlfBuffer, 2);
	    write(pipeTS[1], &crlfBuffer[1], 1);
	  } else {
	    write(1, &buffer[i], 1);
	    write(pipeTS[1], &buffer[i], 1);
	  }
	}
      }

      if(fds[1].revents & POLLIN) {
	count = read(pipeFS[0], buffer, bufferSize);
	
	for(i = 0; i < count; i++) {
	  if(buffer[i] == 26) {
	    break;
	  } else if(buffer[i] == 10) {
	    write(1, crlfBuffer, 2);
	  } else {
	    write(1, &buffer[i], 1);
	  }
	}	  
      }
      
      if(fds[1].revents & POLLHUP) {
	close(pipeFS[0]);
      	break;
      }
      
    }
    
  }

  setOrig(term);

  int status;
  waitpid(0, &status, 0);
  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));

  return 0;
}
  
void setOrig(struct termios term) {
  if (tcsetattr(0, TCSANOW, &term) == -1) {
    fprintf(stderr, "Failed to set terminal to original parameters");
    exit(1);
  }
}

void readWrite(int fdIn, int fdOut) {
  const int bufferSize = 1024;
  int count, i;
  char buffer[bufferSize];
  char crlfBuffer[2];
  crlfBuffer[0] = 13;
  crlfBuffer[1] = 10;

  while(1) {
    count = read(fdIn, buffer, bufferSize);

    for(i = 0; i < count; i++) {
      if(buffer[i] == 4) {
	break;
      } else if(buffer[i] == 13 || buffer[i] == 10) {
	write(fdOut, crlfBuffer, 2);
      } else {
       	write(fdOut, &buffer[i], 1);
      }
    }

    if(buffer[i] == 4)
      break;
  }
}

void sigHandler(int sigNum) {
  if(sigNum == SIGPIPE) {
    setOrig(term);
    int status;
    waitpid(0, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
    exit(0);
  }
}
