#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

void exit_program();
float temp_value(int sensor);
long long time_diff(struct timespec end, struct timespec start);
void get_time();

const long long BILLION = 1000000000;
const int B = 4275;
const int R0 = 100000;

char currTime[9];
mraa_aio_context sensor;

int main(int argc, char** argv) {
	int opt, period, logD, id, port;
	char scale;
	char *periodString, *scaleString, *logString, *idString, *hostString;
	
	period = 1;
	scale = 'F';
	periodString = NULL;
	scaleString = NULL;
	logString = NULL;
	idString = NULL;
	hostString = NULL;

  struct option longOpts[] = {
		{"period", required_argument, NULL, 0},
		{"scale", required_argument, NULL, 1},
		{"log", required_argument, NULL, 2},
		{"id", required_argument, NULL, 3},
		{"host", required_argument, NULL, 4},
		{NULL, 0, NULL, 0}
  };

  while ((opt = getopt_long(argc, argv, "", longOpts, NULL)) != -1) {
    switch (opt) {
    case 0:
      periodString = optarg;
      break;
    case 1:
      scaleString = optarg;
      break;
		case 2:
			logString = optarg;
    	break;
		case 3:
			idString = optarg;
			break;
		case 4:
			hostString = optarg;
			break;
		case '?':
      fprintf(stderr, "Usage: lab4b [--period int] [--scale char] [--log file] [--id int] [--host int] port\n");
      exit(1);
    }
  }

  if(periodString != NULL && ((period = atoi(periodString)) <= 0)) {
    fprintf(stderr, "Invalid period specified\n");
    exit(1);
  }

  if(scaleString != NULL) {
    if(!strcmp(scaleString, "C")) {
      scale = 'C';
    } else if (strcmp(scaleString, "F")) {
      fprintf(stderr, "Invalid scale specified\n");
    }
  }

	if(logString == NULL || (logD = creat(logString, S_IRWXU)) == -1) {
		fprintf(stderr, "Logfile invalid or not specified\n");
		exit(1);
	}

	if(idString == NULL || ((id = atoi(idString)) <= 0)) {
		fprintf(stderr, "ID invalid or not specified\n");
		exit(1);
	}

	if(hostString == NULL) {
		fprintf(stderr, "Host not specified\n");
		exit(1);
	}

	if(argv[optind] == NULL || ((port = atoi(argv[optind])) <= 1024)) {
		fprintf(stderr, "Port invalid or not specified\n");
		exit(1);
	}	

	int sockD, serverD;
	struct hostent* server;
	struct sockaddr_in serverAd;

	if((sockD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Error creating socket\n");
    exit(2);
  }

  if((server = gethostbyname(hostString)) == NULL) {
    fprintf(stderr, "Error getting host information\n");
    exit(1);
  }

  memset((void*)&serverAd, 0, sizeof(serverAd));
  serverAd.sin_family = AF_INET;
  serverAd.sin_port = htons(port);
  serverAd.sin_addr = *((struct in_addr*)server->h_addr);

  if((serverD = connect(sockD, (struct sockaddr*)&serverAd, sizeof(serverAd))) == -1) {
    fprintf(stderr, "Error connecting to remote host\n");
    exit(2);
  }
	
	FILE *sock = fdopen(sockD, "r");
	dprintf(sockD, "ID=%d\n", id);
	dprintf(logD, "ID=%d\n", id);	

	if((sensor = mraa_aio_init(1)) == NULL) {
		fprintf(stderr, "Error initiating sensor\n");
		exit(2);
	}

	int value, tempPeriod, stopFlag;
	const int buffSize = 512;
	char command[buffSize];
	struct timespec startTime, endTime;

	struct pollfd fd[] = {
		{sockD, POLLIN | POLLHUP, 0}
	};

	stopFlag = 0;
  if(clock_gettime(CLOCK_MONOTONIC, &startTime) == -1) {
    fprintf(stderr, "Error getting time\n");
    exit(2);
  }
  while(1) {
    if(clock_gettime(CLOCK_MONOTONIC, &endTime) == -1) {
      fprintf(stderr, "Error getting time\n");
      exit(2);
    }

    if(time_diff(endTime, startTime) >= (((long long) period) * BILLION)) {
      startTime = endTime;

			if((value = mraa_aio_read(sensor)) == -1) {
				fprintf(stderr, "Error reading temperature\n");
				exit(2);
			}
			
			if(!stopFlag) {
				get_time();
				if(scale == 'C') {
      		dprintf(sockD, "%s %.1f\n", currTime, temp_value(value));
					dprintf(logD, "%s %.1f\n", currTime, temp_value(value));
				} else {
					dprintf(sockD, "%s %.1f\n", currTime, (temp_value(value) * 9 / 5) + 32);
					dprintf(logD, "%s %.1f\n", currTime, (temp_value(value) * 9 / 5) + 32);
				}
			}
    }
		
		if(poll(fd, 1, 0) < 0) {
			fprintf(stderr, "Error polling for data");
			exit(2);
		}

		if(fd[0].revents & POLLHUP) {
			fprintf(stderr, "Channel hang up");
			exit(2);
		}

		if(fd[0].revents & POLLIN) {
			if(fgets(command, buffSize, sock) == NULL) {
				fprintf(stderr, "Error getting command");
				exit(2);
			}

			if(!strncmp(command, "SCALE=", 6)) {			
				if(!strcmp(command + 6, "F\n"))
					scale = 'F';
				else if (!strcmp(command + 6, "C\n"))
					scale = 'C';

				dprintf(logD, command);
			} else if (!strncmp(command, "PERIOD=", 7)) {
				tempPeriod = atoi(command + 7);			
				if(tempPeriod > 0)
					period = tempPeriod;

				dprintf(logD, command);
			} else if (!strcmp(command, "STOP\n")) {
				stopFlag = 1;

				dprintf(logD, command);
			} else if (!strcmp(command, "START\n")) {
				stopFlag = 0;

				dprintf(logD, command);
			} else if (!strncmp(command, "LOG ", 4)) {
				dprintf(logD, command);
			} else if (!strcmp(command, "OFF\n")) {
				dprintf(logD, command);

				get_time();

				dprintf(sockD, "%s SHUTDOWN\n", currTime);
				dprintf(logD, "%s SHUTDOWN\n", currTime);
				break;
			}
		}
	}

	mraa_aio_close(sensor);
	shutdown(sockD, SHUT_RDWR);

  return 0;
}

float temp_value(int sensor) {
  float R, temp;

  R = (1023.0 / sensor) - 1.0;
  R = R0 * R;
  temp = (1.0 / ((log(R / R0) / B) + (1 / 298.15))) - 273.15;

  return temp;
}

long long time_diff(struct timespec end, struct timespec start) {
  long long diff;

  diff = ((long long) end.tv_sec - (long long) start.tv_sec) * BILLION +
    ((long long) end.tv_nsec - (long long) start.tv_nsec);

  return diff;
}

void get_time() {
	time_t sec;
	struct tm *realTime;
	
	time(&sec);	
	realTime = localtime(&sec);
	strftime(currTime, 9, "%H:%M:%S", realTime);
}
