#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//  you may need other standard header files


//  CITS2002 Project 1 2023
//  Student1:   STUDENT-NUMBER1   NAME-1
//  Student2:   STUDENT-NUMBER2   NAME-2


//  myscheduler (v1.0)
//  Compile with:  cc -std=c11 -Wall -Werror -o myscheduler myscheduler.c


//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF sysconfig AND command DETAILS
//  THAT YOUR PROGRAM NEEDS TO SUPPORT.  YOU'LL REQUIRE THESE //  CONSTANTS
//  WHEN DEFINING THE MAXIMUM SIZES OF ANY REQUIRED DATA STRUCTURES.

#define MAX_DEVICES                     4
#define MAX_DEVICE_NAME                 20
#define MAX_COMMANDS                    10
#define MAX_COMMAND_NAME                20
#define MAX_SYSCALLS_PER_PROCESS        40
#define MAX_RUNNING_PROCESSES           50

//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

#define DEFAULT_TIME_QUANTUM            100

#define TIME_CONTEXT_SWITCH             5
#define TIME_CORE_STATE_TRANSITIONS     10
#define TIME_ACQUIRE_BUS                20


//  ----------------------------------------------------------------------

#define CHAR_COMMENT                    '#'

struct device {
	char name[MAX_DEVICE_NAME+1];
	int read_speed;
	int write_speed;
} device_list[MAX_DEVICES];

void read_sysconfig(char argv0[], char filename[])
{
	FILE *sysconfig = fopen(filename, "r");
	char line[200];
	char s[3] = " \n";
	char *token;
	if(sysconfig == NULL){
		printf("cannot access file '%s'\n", filename);
	       exit(EXIT_FAILURE);	
	}
	int line_count = 0;
	while( fgets(line, sizeof line, sysconfig) != NULL){
	if(line[0] == '#'){
		continue;
	}
	token= strtok(line, s);
	if(strcmp(token, "timequantum")) {
		continue;
		}
	int device_field = 1;
	while(token != NULL){
		printf( "%s\n", token);
		token = strtok(NULL,s);
		switch (device_field) {
			case 1:
				strcpy(device_list[line_count].name, token);
				break;
			case 2:
				strcpy(device_list[line_count].read_speed, 
	// need to change string field to int
		device_field += 1;
	}
	line_count += 1;	
	}
	}
	fclose(sysconfig);
}

//void read_commands(char argv0[], char filename[])

//{
//}

//  ----------------------------------------------------------------------

//void execute_commands(void)
//{
//}

//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
//  ENSURE THAT WE HAVE THE CORRECT NUMBER OF COMMAND-LINE ARGUMENTS
    if(argc != 3) {
        printf("Usage: %s sysconfig-file command-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

//  READ THE SYSTEM CONFIGURATION FILE
    read_sysconfig(argv[0], argv[1]);

//  READ THE COMMAND FILE
    //read_commands(argv[0], argv[2]);

//  EXECUTE COMMANDS, STARTING AT FIRST IN command-file, UNTIL NONE REMAIN
    //execute_commands();

//  PRINT THE PROGRAM'S RESULTS
    //printf("measurements  %i  %i\n", 0, 0);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4


