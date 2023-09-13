#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//  CITS2002 Project 1 2023
//  Student1:   23062249   ZHIHAO LIN
//  Student2:   23097196   JOSHUA CHUANG

//  myscheduler (v1.0)
//  Compile with:  cc -std=c11 -Wall -Werror -o myscheduler myscheduler.c

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

#define CHAR_COMMENT '#'

//  ----------------------------------------------------------------------

int total_time = 0;
int cpu_time = 0;
int pid_count = 0;
int time_quantum = DEFAULT_TIME_QUANTUM;

//  ----------------------------------------------------------------------
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
  int elapsed_time;
  int children;
  int *times;
  int *syscalls;
  int *io_device;
  int *sleep_time;
  int *io_size;
  int *spawned_process;
};

struct command command_list[MAX_COMMANDS];

struct command ready[MAX_RUNNING_PROCESSES];
int ready_front = -1;
int ready_back = -1;

struct command blocked[MAX_RUNNING_PROCESSES];
int blocked_front = -1;
int blocked_back = -1;

//  ----------------------------------------------------------------------

int check_full(int front, int back)
{
  if ((front == back + 1) || (front == 0 && back == MAX_RUNNING_PROCESSES - 1))
  {
    return 1;
  }
  return 0;
}

int check_empty(int front, int back)
{
  if (front == -1)
  {
    return 1;
  }
  return 0;
}

int enqueue(struct command element, struct command queue[], int *front_p, int *back_p)
{
  if (check_full(*front_p, *back_p))
  {
    printf("Cannot enqueue - queue is full.\n");
    return 0;
  }
  else
  {
    if (*front_p == -1)
    {
      *front_p = 0;
    }
    *back_p = (*back_p + 1) % MAX_RUNNING_PROCESSES;
    memcpy(&queue[*back_p], &element, sizeof queue[*back_p]);
    return 1;
  }
}

int dequeue(struct command *out_element, struct command queue[], int *front_p, int *back_p)
{
  if (check_empty(*front_p, *back_p))
  {
    printf("Cannot dequeue - empty queue.\n");
    return 0;
  }
  else
  {
    memcpy(out_element, &queue[*front_p], sizeof *out_element);
    if (*front_p == *back_p)
    {
      *front_p = -1;
      *back_p = -1;
    }
    else
    {
      *front_p = (*front_p + 1) % MAX_RUNNING_PROCESSES;
    }
    return 1;
  }
}

int syscall_to_int(char scall[])
{
  char *syscalls[] = {"spawn", "read", "write", "sleep", "wait", "exit"};
  for (int i = 0; i < 6; i++)
  {
    if (!strcmp(scall, syscalls[i]))
    {
      return i;
    }
  }
  return -1;
}

int command_to_int(char command_name[])
{
  for (int i = 0; i < MAX_COMMANDS; i++)
  {
    if (!strcmp(command_name, command_list[i].name))
    {
      return i;
    }
  }
  return -1;
}

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

int get_command_lengths(FILE *fp, int lengths[])
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
  return command_count;
}

void get_io_name_size(char string[], int command_index, int line_index)
{
  char device_name[MAX_DEVICE_NAME + 1];
  sscanf(string, "%*s %*s %s %iB", device_name, &command_list[command_index].io_size[line_index]);
  for (int i = 0; i < MAX_DEVICES; i++)
  {
    if (!strcmp(device_list[i].name, device_name))
    {
      command_list[command_index].io_device[line_index] = i;
      break;
    }
  }
}

//  ----------------------------------------------------------------------

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
    if (buffer[0] == CHAR_COMMENT)
    {
      continue;
    }
    else if (buffer[0] == 't')
    {
      int timeq_buffer;
      sscanf("%*s %iusec", &timeq_buffer);
      if (timeq_buffer != time_quantum)
      {
        time_quantum = timeq_buffer;
      }
    }
    trim_line(buffer);

    sscanf(buffer, "%*s %s %iBps %iBps", device_list[d_count].name,
           &device_list[d_count].read_speed, &device_list[d_count].write_speed);

    ++d_count;
  }
  fclose(sysconfig_file);
}

void read_commands(char argv0[], char filename[])
{
  FILE *command_file = fopen(filename, "r");
  if (command_file == NULL)
  {
    printf("Cannot access file - %s\n", filename);
    exit(EXIT_FAILURE);
  }

  int command_lengths[MAX_COMMANDS];
  int num_commands = get_command_lengths(command_file, command_lengths);

  for (int i = 0; i < num_commands; i++)
  {
    int size_array = command_lengths[i] * sizeof(int);
    command_list[i].times = malloc(size_array);
    command_list[i].syscalls = malloc(size_array);
    command_list[i].io_device = calloc(command_lengths[i], sizeof(int));
    command_list[i].sleep_time = calloc(command_lengths[i], sizeof(int));
    command_list[i].io_size = calloc(command_lengths[i], sizeof(int));
    command_list[i].spawned_process = calloc(command_lengths[i], sizeof(int));
    if (command_list[i].times == NULL || command_list[i].syscalls == NULL || command_list[i].io_device == NULL ||
        command_list[i].sleep_time == NULL || command_list[i].io_size == NULL || command_list[i].spawned_process == NULL)
    {
      printf("could not allocate memory\n");
      exit(EXIT_FAILURE);
    }
  }

  char buffer[200];
  int c_count = 0;
  int s_count = 0;
  while (fgets(buffer, sizeof buffer, command_file) != NULL)
  {
    trim_line(buffer);
    if (buffer[0] == CHAR_COMMENT)
    {
      if (s_count == 0)
      {
        continue;
      }
      else
      {
        s_count = 0;
        c_count += 1;
        continue;
      }
    }
    else if (buffer[0] == '\t')
    {
      char syscall[6];
      sscanf(buffer, "%iusecs %s", &command_list[c_count].times[s_count], syscall);
      if (!strcmp(syscall, "read") || !strcmp(syscall, "write"))
      {
        get_io_name_size(buffer, c_count, s_count);
      }
      else if (!strcmp(syscall, "sleep"))
      {
        sscanf(buffer, "%*s %*s %iusecs", &command_list[c_count].sleep_time[s_count]);
      }
      else if (!strcmp(syscall, "spawn"))
      {
        char spawned_process[6];
        sscanf(buffer, "%*s %*s %s", spawned_process);
        command_list[c_count].spawned_process[s_count] = command_to_int(spawned_process);
      }
      command_list[c_count].syscalls[s_count] = syscall_to_int(syscall);
      s_count += 1;
    }
    else
    {
      sscanf(buffer, "%s", command_list[c_count].name);
    }
  }
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
  read_sysconfig(argv[0], argv[1]);

  //  READ THE COMMAND FILE
  read_commands(argv[0], argv[2]);
  //  EXECUTE COMMANDS, STARTING AT FIRST IN command-file, UNTIL NONE REMAIN
  // execute_commands();

  //  PRINT THE PROGRAM'S RESULTS
  // printf("measurements  %i  %i\n", 0, 0);

  exit(EXIT_SUCCESS);
}
