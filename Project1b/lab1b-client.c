#define _GNU_SOURCE
#include <mcrypt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <signal.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>

void setOrig(struct termios term);
void safeWrite(int fd, const void *bf, size_t count);

struct termios term;

int main(int argc, char** argv) {  
  struct termios termCopy;

  if (tcgetattr(0, &term) == -1) {
    fprintf(stderr, "Failed to get terminal parameters\n");
    exit(1);
  }

  termCopy = term;
  termCopy.c_iflag = ISTRIP;
  termCopy.c_oflag = 0;
  termCopy.c_lflag = 0;
  
  int opt, port, logD, keyD, portFlag, logFlag, keyFlag;
  char *portString, *logFile, *keyFile;

  portString = NULL;
  logFile = NULL;
  keyFile = NULL;

  portFlag = 0;
  logFlag = 0;
  keyFlag = 0;

  struct option longOpts[] = {
    {"port", required_argument, NULL, 0},
    {"log", required_argument, NULL, 1},
    {"encrypt", required_argument, NULL, 2},
    {NULL, 0, NULL, 0}
  };

  while ((opt = getopt_long(argc, argv, "", longOpts, NULL)) != -1) {
    switch (opt) {
    case 0:
      portString = optarg;
      portFlag = 1;
      break;
    case 1:
      logFile = optarg;
      logFlag = 1;
      break;
    case 2:
      keyFile = optarg;
      keyFlag = 1;
      break;
    case '?':
      fprintf(stderr, "Usage: lab1a --port number [--log file] [--encrypt file]\n");
      exit(1);
    }
  }

  if((port = atoi(portString)) == 0 || port <= 1024 || !portFlag) {
    fprintf(stderr, "Invalid port specified\n");
    exit(1);
  }

  if(logFile != NULL && (logD = creat(logFile, S_IRWXU)) == -1) {
    fprintf(stderr, "Log file error (%s): %s\n", logFile, strerror(errno));
    exit(1);
  }

  if(keyFile != NULL && (keyD = open(keyFile, O_RDONLY)) == -1) {
    fprintf(stderr, "Key file error (%s): %s\n", keyFile, strerror(errno));
    exit(1);
  }
  
  int sockD, serverD;
  struct hostent* server;
  struct sockaddr_in serverAd;

  if((sockD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Error creating socket\n");
    exit(1);
  }

  if((server = gethostbyname("localhost")) == NULL) {
    fprintf(stderr, "Error getting host inf\n");
    exit(1);
  }

  memset((void*)&serverAd, 0, sizeof(serverAd));
  serverAd.sin_family = AF_INET;
  serverAd.sin_port = htons(port);
  serverAd.sin_addr = *((struct in_addr*)server->h_addr);

  if((serverD = connect(sockD, (struct sockaddr*)&serverAd, sizeof(serverAd))) == -1) {
    fprintf(stderr, "Error connecting to remote host\n");
    exit(1);
  }
  
  struct pollfd fds[] = {
    {0, POLLIN, 0},
    {sockD, POLLIN | POLLRDHUP, 0}
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
  int count;
  char buffer[bufferSize];
  char crlfBuffer[2];
  crlfBuffer[0] = 13;
  crlfBuffer[1] = 10;

  if (tcsetattr(0, TCSANOW, &termCopy) == -1) {
    fprintf(stderr, "Failed to set terminal to new parameters\n");
    exit(1);
  }

  while(1) {
    if(poll(fds, 2, -1) == -1) {
      setOrig(term);
      fprintf(stderr, "Failure to poll for data\n");
      exit(1);
    }
    
    if(fds[1].revents & POLLRDHUP) {
      break;
    }

    if(fds[0].revents & POLLERR || fds[0].revents & POLLHUP || fds[0].revents & POLLNVAL) {
      setOrig(term);
      fprintf(stderr, "Failure to poll for keyboard input\n");
      exit(1);
    }
    
    if(fds[1].revents & POLLERR || fds[1].revents & POLLHUP || fds[1].revents & POLLNVAL) {
      setOrig(term);
      fprintf(stderr, "Failure to poll for server output\n");
      exit(1);
    }
    
    if(fds[0].revents & POLLIN) {
      count = read(0, buffer, bufferSize);

      for(i = 0; i < count; i++) {
	if(buffer[i] == 13 || buffer[i] == 10) {
	  write(1, crlfBuffer, 2);
	  
	  if(keyFlag) {
	    if(mcrypt_generic(enD, &crlfBuffer[1], 1) != 0) {
	      setOrig(term);
	      fprintf(stderr, "Error encrypting\n");
	      exit(1);
	    }

	    write(sockD, &crlfBuffer[1], 1);
	    crlfBuffer[1] = 10;
	  } else {
	    write(sockD, &crlfBuffer[1], 1);
	  }
	} else {
	  write(1, &buffer[i], 1);
	  
	  if(keyFlag && (mcrypt_generic(enD, &buffer[i], 1) != 0)) {
	    setOrig(term);
	    fprintf(stderr, "Error encrypting\n");
	    exit(1);
	  }
	  
	  write(sockD, &buffer[i], 1);
	}
      }

      if(logFlag) {
 	if(dprintf(logD, "SENT %d bytes: ", count) < 0) {
	  setOrig(term);
	  fprintf(stderr, "Error printing to log");
	  exit(1);
	}
	
	safeWrite(logD, buffer, count);
	
	if(dprintf(logD, "\n") < 0) {
	  setOrig(term);
	  fprintf(stderr, "Error printing to log");
	  exit(1);
	}
      }
    }

    if(fds[1].revents & POLLIN) {
      count = read(sockD, buffer, bufferSize);

      if(logFlag) {
	if(dprintf(logD, "RECEIVED %d bytes: ", count) < 0) {
	  setOrig(term);
	  fprintf(stderr, "Error printing to log");
	  exit(1);
	}
	
	safeWrite(logD, buffer, count);
	
	if(dprintf(logD, "\n") < 0) {
	  setOrig(term);
	  fprintf(stderr, "Error printing to log");
	  exit(1);
	}
      }

      if(keyFlag && (mdecrypt_generic(deD, buffer, count) != 0)) {
	setOrig(term);	
	fprintf(stderr, "Error decrypting\n");
	exit(1);
      }

      write(1, buffer, count);
    }
  }  

  shutdown(sockD, SHUT_RDWR);
  setOrig(term);

  return 0;
}

void setOrig(struct termios term) {
  if (tcsetattr(0, TCSANOW, &term) == -1) {
    fprintf(stderr, "Failed to set terminal to original parameters\n");
    exit(1);
  }
}

void safeWrite(int fd, const void *buf, size_t count) {
  if(write(fd, buf, count) == -1) {
    setOrig(term);
    fprintf(stderr, "Write failed with error number: %d", errno);
    exit(1);
  }
}
