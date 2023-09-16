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
//  Variables for global timer.

enum process_status
{
  ready,
  running,
  blocked,
  exited
};

enum databus_status
{
  vacant,
  occupied
};

enum databus_status databus;

int total_time = 0;
int cpu_time = 0;
int process_count = 0;
int time_quantum = DEFAULT_TIME_QUANTUM;

//  ----------------------------------------------------------------------
//  Data structures to store sysconfig and command text file data.
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
  enum process_status status;
  int on_cpu;
  // Store block end time in here, once CPU is idle, check blocked queue for finished blocks.
  // block_end time is set as soon as sleep is called, -1 if wait called
  int block_end;
  // Store io total time here, as io end time cannot be set as soon as read/write called,
  // rather when io commences. Add to block_end once io commences
  int io_time;
  int io_end;
  // Store number of children, for processes that wait for child processes.
  int children;
  int *times;
  enum syscall_type *syscalls;
  int *io_device;
  int *sleep_time;
  int *io_size;
  int *spawned_process;
};

struct command command_list[MAX_COMMANDS];

enum syscall_type
{
  _spawn_,
  _read_,
  _write_,
  _sleep_,
  _wait_,
  _exit_
};

//  ----------------------------------------------------------------------
//  Queue functions and definitions.

struct command ready_queue[MAX_RUNNING_PROCESSES];
int ready_front = -1;
int ready_back = -1;

// Queue for processes blocked for sleep or wait calls
struct command blocked_queue[MAX_RUNNING_PROCESSES];
int blocked_front = -1;
int blocked_back = -1;

// Queue for process blocked for io calls
struct command io_queue[MAX_RUNNING_PROCESSES];
int io_front = -1;
int io_back = -1;

int check_full(int front, int back)
{
  if ((front == back + 1) || (front == 0 && back == MAX_RUNNING_PROCESSES - 1))
  {
    return 1;
  }
  return 0;
}

int check_empty(int front)
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
  if (check_empty(*front_p))
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

// Requeues front element.
int requeue(struct command queue[], int *front_p, int *back_p)
{
  struct command buf;
  dequeue(&buf, queue, front_p, back_p);
  enqueue(buf, queue, front_p, back_p);
  return 1;
}

//  ----------------------------------------------------------------------
//  Helper functions for storing text file data.

int syscall_to_int(char syscall[])
{
  char *syscall_strs[] = {"spawn", "read", "write", "sleep", "wait", "exit"};
  for (int i = 0; i < 6; i++)
  {
    if (!strcmp(syscall, syscall_strs[i]))
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
//  Functions to read text files.

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
      sscanf(buffer, "%*s %iusec", &timeq_buffer);
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
    size_t size_array = command_lengths[i] * sizeof(int);
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
      char syscall_str[6];
      sscanf(buffer, "%iusecs %s", &command_list[c_count].times[s_count], syscall_str);
      int syscall_int = syscall_to_int(syscall_str);
      enum syscall_type syscall = (enum syscall_type)syscall_int;
      switch (syscall)
      {
      case _read_:
      case _write_:
        get_io_name_size(buffer, c_count, s_count);
        break;
      case _sleep_:
        sscanf(buffer, "%*s %*s %iusecs", &command_list[c_count].sleep_time[s_count]);
        break;
      case _spawn_:
        char spawned_process[6];
        sscanf(buffer, "%*s %*s %s", spawned_process);
        command_list[c_count].spawned_process[s_count] = command_to_int(spawned_process);
        break;
      default:
        break;
      }
      command_list[c_count].syscalls[s_count] = syscall;
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
//  Helper functions for execution emulation.

void new_process(struct command *buffer, struct command *template)
{
  memcpy(buffer, template, sizeof *buffer);
  (*buffer).pid = process_count;
  (*buffer).status = ready;
  printf("Time - %i\n", total_time);
  printf("Spawning new process %s, pid-%i, new -> ready, transition 0usecs\n", (*buffer).name, (*buffer).pid);
  process_count += 1;
}

void call_exit(struct command *process)
{
  // Decrement children field of parent process
  total_time += 1;
  int parent_id = (*process).ppid;
  for (int i = io_front; i != (io_back + 1) % MAX_RUNNING_PROCESSES; i = (i + 1) % MAX_RUNNING_PROCESSES)
  {
    if (io_queue[i].pid == parent_id)
    {
      io_queue[i].children -= 1;
      // If children is 0, then parent process can be unblocked by idle CPU later
      if (io_queue[i].children == 0 && io_queue[i].block_end == -1)
      {
        io_queue[i].block_end = 0;
      }
      break;
    }
  }
  printf("Time - %i\n", total_time);
  printf("Process %s, pid - %i running -> exit, transition 0usecs\n", (*process).name, (*process).pid);
  (*process).status = exited;
  struct command buf;
  dequeue(&buf, ready_queue, &ready_front, &ready_back);
}

void call_spawn(int line, struct command *process)
{
  total_time += 1;
  struct command buf;
  int to_spawn = (*process).spawned_process[line];
  new_process(&buf, &command_list[to_spawn]);
  buf.ppid = (*process).pid;
  (*process).children += 1;

  enqueue(buf, ready_queue, &ready_front, &ready_back);
  requeue(ready_queue, &ready_front, &ready_back);
  printf("Time - %i\n", total_time);
  printf("Process %s, pid - %i running -> ready, transition 10usecs\n", (*process).name,
         (*process).pid);
  total_time += TIME_CORE_STATE_TRANSITIONS;
}

void call_sleep(int line, struct command *process)
{
  total_time += 1;
  int sleep_time = (*process).sleep_time[line];
  (*process).block_end = total_time + sleep_time + 1;
  printf("Time - %i\n", total_time);
  printf("Process %s, pid - %i, %iusecs sleep, running -> sleeping, transition 10usecs\n", (*process).name,
         (*process).pid, sleep_time);
  total_time += TIME_CORE_STATE_TRANSITIONS;
  struct command buf;
  dequeue(&buf, ready_queue, &ready_front, &ready_back);
  enqueue(buf, blocked_queue, &blocked_front, &blocked_back);
}

void call_wait(struct command *process)
{
  total_time += 1;
  printf("Time - %i\n", total_time);
  printf("Process %s, pid - %i, running -> wait, transition 10usecs\n", (*process).name, (*process).pid);
  total_time += TIME_CORE_STATE_TRANSITIONS;

  (*process).block_end = -1;
  struct command buf;
  dequeue(&buf, ready_queue, &ready_front, &ready_back);
  enqueue(buf, blocked_queue, &blocked_front, &blocked_back);
}

void call_io(int line, struct command *process, enum syscall_type io_type)
{
  total_time += 1;
  int device = (*process).io_device[line];
  int size = (*process).io_size[line];
  switch (io_type)
  {
  case _read_:
    int rspeed = device_list[device].read_speed;
    int rtime = size / rspeed;
    (*process).io_time = rtime;
    printf("Time - %i\n", total_time);
    printf("Reading %ibytes, pid - %i, running -> blocked, transition 10usecs\n", size, (*process).pid);
    break;
  case _write_:
    int wspeed = device_list[device].write_speed;
    int wtime = size / wspeed;
    (*process).io_time = wtime;
    printf("Time - %i\n", total_time);
    printf("Writing %ibytes, pid - %i, running -> blocked, transition 10usecs\n", size, (*process).pid);
    break;
  default:
    printf("Invalid io call type.\n");
    exit(EXIT_FAILURE);
  }

  total_time += TIME_CORE_STATE_TRANSITIONS;
  struct command buf;
  dequeue(&buf, ready_queue, &ready_front, &ready_back);
  enqueue(buf, io_queue, &io_front, &io_back);
}

void handle_syscall(int line, struct command *process)
{
  enum syscall_type syscall = (*process).syscalls[line];
  switch (syscall)
  {
  case _spawn_:
    call_spawn(line, process);
    break;
  case _read_:
  case _write_:
    call_io(line, process, syscall);
    break;
  case _sleep_:
    call_sleep(line, process);
    break;
  case _wait_:
    call_wait(process);
    break;
  case _exit_:
    call_exit(process);
    break;
  }
}

void one_time_quantum(void)
{
  struct command *front = &ready_queue[ready_front];

  (*front).status = running;
  printf("Time - %i\n", total_time);
  printf("Running process %s, pid-%i, ready -> running, transition 5usecs\n",
         (*front).name, (*front).pid);
  total_time += TIME_CONTEXT_SWITCH;
  //  Variable to determine which "line" of the command we are on
  int line = 0;
  for (int i = 0;; i++)
  {
    if ((*front).times[i] > (*front).on_cpu)
    {
      line = i;
      break;
    }
  }
  printf("Time - %i\n", total_time);
  printf("Process %s now running\n", (*front).name);

  int computation = (*front).times[line] - (*front).on_cpu;
  if (computation > time_quantum)
  {
    (*front).on_cpu += time_quantum;
    cpu_time += time_quantum;
    total_time += time_quantum;
    printf("Time quantum expired, process %s pid - %i, running -> ready 10usecs\n",
           (*front).name, (*front).pid);
    total_time += TIME_CORE_STATE_TRANSITIONS;
    (*front).status = ready;
    requeue(ready_queue, &ready_front, &ready_back);
  }
  else
  {
    (*front).on_cpu += computation;
    cpu_time += computation;
    total_time += computation;
    handle_syscall(line, front);
  }
  return;
}

void unblock_sleep(void)
{
  for (int i = blocked_front; i != (blocked_back + 1) % MAX_RUNNING_PROCESSES && !check_empty(blocked_front); i = (i + 1) % MAX_RUNNING_PROCESSES)
  {
    if (blocked_queue[i].block_end == -1)
    {
      requeue(blocked_queue, &blocked_front, &blocked_back);
      continue;
    }
    if (blocked_queue[i].block_end <= total_time)
    {
      struct command buf;
      dequeue(&buf, blocked_queue, &blocked_front, &blocked_back);
      enqueue(buf, ready_queue, &ready_front, &ready_back);
      printf("Time - %i\n", total_time);
      printf("pid - %i waking, sleeping -> ready, transition 10usecs\n", buf.pid);
      total_time += TIME_CORE_STATE_TRANSITIONS;
    }
  }
}

void unblock_wait(void)
{
}

void unblock_io(void)
{
}

void commence_io(void)
{
}

int all_queues_empty(void)
{
  if (check_empty(ready_front) && check_empty(blocked_front) && check_empty(io_front))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
//  ----------------------------------------------------------------------
//  Main driver function.

void execute_commands(void)
{
  struct command command_buffer;
  new_process(&command_buffer, &command_list[0]);
  enqueue(command_buffer, ready_queue, &ready_front, &ready_back);

  while (!all_queues_empty())
  {
    if (!check_empty(ready_front))
    {
      one_time_quantum();
    }

    // Implement idle cpu priorities here.
    unblock_sleep();
    unblock_wait();
    unblock_io();
    commence_io();

    // if nothing was unblocked, increment time, until something is unblocked.
    if (check_empty(ready_front) && databus == vacant)
    {
      total_time += 1;
    }
  }
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
  execute_commands();

  //  PRINT THE PROGRAM'S RESULTS
  int percentage = cpu_time * 100 / total_time;
  printf("measurements  %i  %i\n", total_time, percentage);

  exit(EXIT_SUCCESS);
}
