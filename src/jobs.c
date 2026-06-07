#include "jobs.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

// Builtin handler for the "jobs" command
void builtin_jobs(void) {
  // Pass 1: mark exited jobs as Done (don't print yet)
  for (int i = 0; i < job_count; i++) {
    int status; // wait status
    pid_t result = waitpid(job_table[i].pid, &status, WNOHANG);
    if (result > 0 && WIFEXITED(status)) { job_table[i].status = "Done"; }
  }

  // Pass 2: print all jobs in original order (Running and Done interleaved)
  int total = job_count;
  for (int i = 0; i < total; i++) {
    char marker = (i == total-1) ? '+' : (i == total-2) ? '-' : ' ';
    bool done = (strcmp(job_table[i].status, "Done") == 0);
    char *cmd = job_table[i].cmd;
    size_t cmdlen = strlen(cmd);
    if (done && cmdlen >= 2 && strcmp(cmd+cmdlen-2, " &") == 0) {
      printf("[%d]%c  %-24s%.*s\n", job_table[i].job_num, marker, "Done", (int)(cmdlen-2), cmd);
    } else {
      printf("[%d]%c  %-24s%s\n", job_table[i].job_num, marker, job_table[i].status, cmd);
    }
  }

  // Pass 3: compact table, free Done entries
  int new_count = 0;
  for (int i = 0; i < job_count; i++) {
    if (strcmp(job_table[i].status, "Done") == 0) {
      free(job_table[i].cmd);
    } else {
      job_table[new_count++] = job_table[i];
    }
  }
  job_count = new_count;
}
