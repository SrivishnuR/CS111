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

void exit_program();
float temp_value(int sensor);
long long time_diff(struct timespec end, struct timespec start);
void get_time();

const long long BILLION = 1000000000;
const int B = 4275;
const int R0 = 100000;

char currTime[9];
int logD, logFlag;
mraa_aio_context sensor;
mraa_gpio_context button;

int main(int argc, char** argv) {
  int opt, period;
  char scale;
  char *periodString, *scaleString, *logString;

  period = 1;
	logFlag = 0;
  scale = 'F';
  periodString = NULL;
  scaleString = NULL;
	logString = NULL;

  struct option longOpts[] = {
    {"period", required_argument, NULL, 0},
    {"scale", required_argument, NULL, 1},
		{"log", required_argument, NULL, 2},
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
			logFlag = 1;
    	break;
		case '?':
      fprintf(stderr, "Usage: lab4b [--period int] [--scale char]\n");
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

	if(logString != NULL && (logD = creat(logString, S_IRWXU)) == -1) {
		fprintf(stderr, "Invalid log specification\n");
		exit(1);
	}
  
	if((sensor = mraa_aio_init(1)) == NULL) {
		fprintf(stderr, "Error initiating sensor\n");
		exit(1);
	}

	if((button = mraa_gpio_init(60)) == NULL) {
		fprintf(stderr, "Error initiating button\n");	
		exit(1);
	}
  
	mraa_gpio_dir(button, MRAA_GPIO_IN);
  mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &exit_program, NULL);

	int value, tempPeriod, stopFlag;
	const int buffSize = 512;
	char command[buffSize];
	struct timespec startTime, endTime;

	struct pollfd fd[] = {
		{STDIN_FILENO, POLLIN, 0}
	};

	stopFlag = 0;
  if(clock_gettime(CLOCK_MONOTONIC, &startTime) == -1) {
    fprintf(stderr, "Error getting time\n");
    exit(1);
  }
  while(1) {
    if(clock_gettime(CLOCK_MONOTONIC, &endTime) == -1) {
      fprintf(stderr, "Error getting time\n");
      exit(1);
    }

    if(time_diff(endTime, startTime) >= (((long long) period) * BILLION)) {
      startTime = endTime;

			if((value = mraa_aio_read(sensor)) == -1) {
				fprintf(stderr, "Error reading temperature\n");
				exit(1);
			}
			
			if(!stopFlag) {
				get_time();
				if(scale == 'C') {
      		printf("%s %.1f\n", currTime, temp_value(value));
					
					if(logFlag)
						dprintf(logD, "%s %.1f\n", currTime, temp_value(value));
				} else {
					printf("%s %.1f\n", currTime, (temp_value(value) * 9 / 5) + 32);
				
					if(logFlag)
						dprintf(logD, "%s %.1f\n", currTime, (temp_value(value) * 9 / 5) + 32);
				}
			}
    }
		
		if(poll(fd, 1, 0) < 0) {
			fprintf(stderr, "Error polling for data");
			exit(1);
		}

		if(fd[0].revents & POLLIN) {
			if(fgets(command, buffSize, stdin) == NULL) {
				fprintf(stderr, "Error getting command");
				exit(1);
			}

			if(!strncmp(command, "SCALE=", 6)) {			
				if(!strcmp(command + 6, "F\n"))
					scale = 'F';
				else if (!strcmp(command + 6, "C\n"))
					scale = 'C';

				if(logFlag)
					dprintf(logD, command);
			} else if (!strncmp(command, "PERIOD=", 7)) {
				tempPeriod = atoi(command + 7);			
				if(tempPeriod > 0)
					period = tempPeriod;

				if(logFlag)
					dprintf(logD, command);
			} else if (!strcmp(command, "STOP\n")) {
				stopFlag = 1;

				if(logFlag)
					dprintf(logD, command);
			} else if (!strcmp(command, "START\n")) {
				stopFlag = 0;

				if(logFlag)
					dprintf(logD, command);
			} else if (!strncmp(command, "LOG ", 4)) {
				if(logFlag)
					dprintf(logD, command);
			} else if (!strcmp(command, "OFF\n")) {
				if(logFlag)
					dprintf(logD, command);
	
				exit_program();
			}
		}
	}

	mraa_aio_close(sensor);
	mraa_gpio_close(button);

  return 0;
}

void exit_program() {
	get_time();
	printf("%s SHUTDOWN\n", currTime);

	if(logFlag)
		dprintf(logD, "%s SHUTDOWN\n", currTime);

	mraa_aio_close(sensor);
	mraa_gpio_close(button);
	exit(0);
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
