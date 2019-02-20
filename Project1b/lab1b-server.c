#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <mcrypt.h>

void sigHandler(int sigNum);
void safeWrite(int fd, const void *buf, size_t count);

int sockD, connD;

int main(int argc, char** argv) {
  int opt, port, keyD, portFlag, keyFlag;
  char *portString, *keyFile;

  portString = NULL;
  keyFile = NULL;

  portFlag = 0;
  keyFlag = 0;

  struct option longOpts[] = {
    {"port", required_argument, NULL, 0},
    {"encrypt", required_argument, NULL, 1},
    {NULL, 0, NULL, 0}
  };

  while ((opt = getopt_long(argc, argv, "", longOpts, NULL)) != -1) {
    switch (opt) {
    case 0:
      portString = optarg;
      portFlag = 1;
      break;
    case 1:
      keyFile = optarg;
      keyFlag = 1;
      break;
    case '?':
      fprintf(stderr, "Usage: lab1a --port number [--encrypt file]\n");
      exit(1);
    }
  }

  if((port = atoi(portString)) == 0 || port <= 1024 || !portFlag) {
    fprintf(stderr, "Invalid port specified\n");
    exit(1);
  }

  if(keyFile != NULL && (keyD = open(keyFile, O_RDONLY)) == -1) {
    fprintf(stderr, "Key file error (%s): %s\n", keyFile, strerror(errno));
    exit(1);
  }
  
  struct sockaddr_in serverAd, inAd;
  socklen_t inLen;

  if((sockD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Error creating socket\n");
    exit(1);
  }

  memset(&serverAd, 0, sizeof(serverAd));
  serverAd.sin_family = AF_INET;
  serverAd.sin_port = htons(port);
  serverAd.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sockD, (struct sockaddr*)&serverAd, sizeof(serverAd)) == -1) {
    fprintf(stderr, "Error binding socket\n");
    exit(1);
  }

  if(listen(sockD, 5) == -1) {
    fprintf(stderr, "Error with listen\n");
    exit(1);
  }

  if((connD = accept(sockD, (struct sockaddr*)&inAd, &inLen)) == -1) {
    fprintf(stderr, "Error with connect\n");
    exit(1);
  }
  
  int pipeTS[2], pipeFS[2];
  pid_t id;

  if(pipe(pipeTS) == -1 || pipe(pipeFS) == -1) {
    fprintf(stderr, "Error creating pipes\n");
    exit(1);
  }
  
  if((id = fork()) == -1) {
    fprintf(stderr, "Error creating child process\n");
    exit(1);
  }

  if(id == 0) {
    dup2(pipeTS[0], 0);
    dup2(pipeFS[1], 1);
    dup2(pipeFS[1], 2);
    
    if(close(pipeTS[0]) == -1 || close(pipeTS[1]) == -1 || 
       close(pipeFS[0]) == -1 || close(pipeFS[1]) == -1) {
      fprintf(stderr, "Error closing pipes\n");
      exit(1);
    }

    if(execl("/bin/bash", "sh", (char*)NULL) == -1) {
      fprintf(stderr, "Error with executing shell\n");
      exit(1);
    }
  } else {
    if(signal(SIGPIPE, sigHandler) == SIG_ERR) {
      fprintf(stderr, "Error setting up signal handler\n");
      exit(1);
    }

    if(close(pipeTS[0]) == -1 || close(pipeFS[1]) == -1) {
      fprintf(stderr, "Error closing pipes\n");
      exit(1);
    }
    
    struct pollfd fds[] = {
      {connD, POLLIN, 0},
      {pipeFS[0], POLLIN, 0}
    };

    int i;
    MCRYPT enD, deD;

    if(keyFlag) {
      int keySize, ivSize;
      char *keyBuf, *ivBuf;
      struct stat fileInf;
      
      if(fstat(keyD, &fileInf) == -1) {
	fprintf(stderr, "Error opening key file information");
	exit(1);
      }

      keySize = fileInf.st_size;

      if((keyBuf = (char*)malloc(keySize * sizeof(char))) == NULL) {
	fprintf(stderr, "Error allocating memory");
	exit(1);
      }

      if(read(keyD, keyBuf, keySize) == -1) {
	fprintf(stderr, "Error reading key file");
	exit(1);
      }

      if((enD = mcrypt_module_open("twofish", NULL, "cfb", NULL)) == MCRYPT_FAILED) {
	fprintf(stderr, "Error opening mcrypt");
	exit(1);
      }

      if((deD = mcrypt_module_open("twofish", NULL, "cfb", NULL)) == MCRYPT_FAILED) {
	fprintf(stderr, "Error opening mcrypt");
	exit(1);
      }
      
      if(keySize >= mcrypt_enc_get_key_size(enD)) {
	fprintf(stderr, "Key size is too large");
	exit(1);
      }
      
      ivSize = mcrypt_enc_get_iv_size(enD);

      if((ivBuf = (char*)malloc(ivSize * sizeof(char))) == NULL) {
	fprintf(stderr, "Error allocating memory");
	exit(1);
      }
      
      for(i = 0; i < ivSize; i++)
	ivBuf[i] = i;
      
      if(mcrypt_generic_init(enD, keyBuf, keySize, ivBuf) < 0) {
	fprintf(stderr, "Error initiating mcrypt");
	exit(1);
      }

      if(mcrypt_generic_init(deD, keyBuf, keySize, ivBuf) < 0) {
	fprintf(stderr, "Error initiating mcrypt");
	exit(1);
      }
    }

    const int bufferSize = 1024;
    int count, eofFlag;
    char buffer[bufferSize];
    char crlfBuffer[2];
    crlfBuffer[0] = 13;
    crlfBuffer[1] = 10;

    while(1) {
      if(poll(fds, 2, -1) == -1) {
	fprintf(stderr, "Error polling for data\n");
	exit(1);
      }

      if(fds[0].revents & POLLERR || fds[0].revents & POLLNVAL) {
	fprintf(stderr, "Error polling for client input\n");
       	exit(1);
      }

      if(fds[1].revents & POLLERR || fds[1].revents & POLLNVAL) {
	fprintf(stderr, "Error polling for shell output\n");
	exit(1);
      }

      if(fds[0].revents & POLLHUP) {
	close(pipeTS[1]);
      }

      if(fds[0].revents & POLLIN) {
	count = read(connD, buffer, bufferSize);

	if(keyFlag && (mdecrypt_generic(deD, buffer, count) != 0)) {
	  fprintf(stderr, "Error decrypting\n");
	  exit(1);
	}

	for(i = 0; i < count; i++) {
	  if(buffer[i] == 3) {
	    if(kill(id, SIGINT) == -1) {
	      fprintf(stderr, "Error killing child process\n");
	      exit(1);
	    } 
	  } else if(buffer[i] == 4) {
	    if(close(pipeTS[1]) == -1) {
	      fprintf(stderr, "Error closing pipes\n");
	      exit(1);
	    }
	  } else {
	    //safeWrite(1, &buffer[i], 1);
	    safeWrite(pipeTS[1], &buffer[i], 1);
	  }
	}
      }

      if(fds[1].revents & POLLIN) {
	count = read(pipeFS[0], buffer, bufferSize);

	if(count == 0) {
	  eofFlag = 1;
	}

	for(i = 0; i < count; i++) {
	  if(buffer[i] == 10) {
	    //	    safeWrite(1, crlfBuffer, 2);
	    
	    if(keyFlag) {
	      if(mcrypt_generic(enD, crlfBuffer, 2) != 0) {
		fprintf(stderr, "Error encrypting\n");
		exit(1);
	      }
	      
	      write(connD, crlfBuffer, 2);
	      crlfBuffer[0] = 13;
	      crlfBuffer[1] = 10;
	    } else {
	      safeWrite(connD, crlfBuffer, 2);
	    }
	  } else {
	    // safeWrite(1, &buffer[i], 1);
	    
	    if(keyFlag && (mcrypt_generic(enD, &buffer[i], 1) != 0)) {
	      fprintf(stderr, "Error encrypting\n");
	      exit(1);
	    }
	    
	    safeWrite(connD, &buffer[i], 1);
	  }
	}
      }
      
      if(eofFlag || (fds[1].revents & POLLHUP)) {
	close(pipeFS[0]);
      	break;
      }
    }
  }

  int status;
  waitpid(0, &status, 0);
  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
  shutdown(sockD, SHUT_RDWR);
  shutdown(connD, SHUT_RDWR);

  return 0;
}

void sigHandler(int sigNum) {
  if(sigNum == SIGPIPE) {
    int status;
    waitpid(0, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
    shutdown(sockD, SHUT_RDWR);
    shutdown(connD, SHUT_RDWR);

    exit(0);
  }
}

void safeWrite(int fd, const void *buf, size_t count) {
  if(write(fd, buf, count) == -1) {
    fprintf(stderr, "Write failed with error number: %d", errno);
    exit(1);
  }
}

