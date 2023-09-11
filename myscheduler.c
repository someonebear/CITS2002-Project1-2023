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

#define MAX_DEVICES 4
#define MAX_DEVICE_NAME 20
#define MAX_COMMANDS 10
#define MAX_COMMAND_NAME 20
#define MAX_SYSCALLS_PER_PROCESS 40
#define MAX_RUNNING_PROCESSES 50

//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

#define DEFAULT_TIME_QUANTUM 100

#define TIME_CONTEXT_SWITCH 5
#define TIME_CORE_STATE_TRANSITIONS 10
#define TIME_ACQUIRE_BUS 20

//  ----------------------------------------------------------------------

#define CHAR_COMMENT '#'

struct device
{
  char name[MAX_DEVICE_NAME + 1];
  int read_speed;
  int write_speed;
};

struct device device_list[MAX_DEVICES];

struct command
{
  char name[MAX_COMMAND_NAME + 1];
  int pid;
  int ppid;
  int status;
  int *times;
  int *syscalls;
  int *device;
  int *sleep_time;
  int *io_size;
};

struct command command_list[MAX_COMMANDS];

void trim_line(char line[])
{
  int i = 0;
  while (line[i] != '\0')
  {
    if (line[i] == '\r' || line[i] == '\n')
    {
      line[i] = '\0';
      break;
    }
    i += 1;
  }
}

void read_sysconfig(char argv0[], char filename[])
{
  FILE *sysconfig_file = fopen(filename, "r");
  if (sysconfig_file == NULL)
  {
    printf("Cannot access file - %s\n", filename);
    exit(EXIT_FAILURE);
  }

  char buffer[200];
  int d_count = 0;
  while (fgets(buffer, sizeof buffer, sysconfig_file) != NULL)
  {
    if (buffer[0] == CHAR_COMMENT || buffer[0] == 't')
    {
      continue;
    }
    trim_line(buffer);

    sscanf(buffer, "%*s %s %iBps %iBps", device_list[d_count].name,
           &device_list[d_count].read_speed, &device_list[d_count].write_speed);

    // printf("Device %i,\n%s %i %i\n", d_count, device_list[d_count].name,
    //        device_list[d_count].read_speed, device_list[d_count].write_speed);
    ++d_count;
  }
  fclose(sysconfig_file);
}

void get_command_lengths(FILE *fp, int lengths[])
{
  char buffer[200];
  int command_count = 0;
  int syscall_count = 0;
  while (fgets(buffer, sizeof buffer, fp) != NULL)
  {
    if (buffer[0] == CHAR_COMMENT)
    {
      if (syscall_count == 0)
      {
        continue;
      }
      else
      {
        lengths[command_count] = syscall_count;
        syscall_count = 0;
        command_count += 1;
      }
    }
    if (buffer[0] == '\t')
    {
      syscall_count += 1;
    }
  }
  fseek(fp, 0, SEEK_SET);
}

void read_commands(char argv0[], char filename[])
{
  FILE *command_file = fopen(filename, "r");
  if (command_file == NULL)
  {
    printf("Cannot access file - %s\n", filename);
    exit(EXIT_FAILURE);
  }

  char buffer[200];
  int command_lengths[MAX_COMMANDS];
  int command_count = 0;
  get_command_lengths(command_file, command_lengths);

  while (fgets(buffer, sizeof buffer, command_file) != NULL)
  {
  }

  printf("Command name is %s\n", buffer);
  fclose(command_file);
}

//  ----------------------------------------------------------------------

void execute_commands(void)
{
}

//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
  //  ENSURE THAT WE HAVE THE CORRECT NUMBER OF COMMAND-LINE ARGUMENTS
  if (argc != 3)
  {
    printf("Usage: %s sysconfig-file command-file\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  //  READ THE SYSTEM CONFIGURATION FILE
  // read_sysconfig(argv[0], argv[1]);

  //  READ THE COMMAND FILE
  read_commands(argv[0], argv[2]);

  //  EXECUTE COMMANDS, STARTING AT FIRST IN command-file, UNTIL NONE REMAIN
  // execute_commands();

  //  PRINT THE PROGRAM'S RESULTS
  // printf("measurements  %i  %i\n", 0, 0);

  exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
