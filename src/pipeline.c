#include "pipeline.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_STAGES 64

// AUX: Run one pipeline cmd in a child
// dup2 for stdin/stdout has already been done by the caller before this runs
static void run_child(char **argv, int is_builtin_cmd) {
  if (is_builtin_cmd) {
    int argc = 0;
    while (argv[argc]) { argc++; }
    execute_builtin(argc, argv);
    _exit(0);
  } else {
    char *path = find_executable_in_path(argv[0]);
    if (!path) {
      fprintf(stderr, "%s: command not found\n", argv[0]);
      _exit(1);
    }
    execv(path, argv);
    perror("execv");
    _exit(1);
  }
}

// Execute an N-stage pipeline from a flat token array
// Splits on "|" tokens, creates N-1 pipes, forks N children
void pipeline(char *args[], int argc) {
  // 1. Split token array into per-stage argv[] pointers by null-ing "|" tokens
  char **stages[MAX_STAGES];
  int n = 0; // number of stages

  stages[n++] = args; // first stage starts at args[0]
  for (int i = 0; i < argc; i++) {
    if (strcmp(args[i], "|") == 0) {
      args[i] = NULL; // null-term curr stage's argv
      if (n >= MAX_STAGES) { fprintf(stderr, "too many pipeline stages\n"); return; }
      stages[n++] = args+i+1; // next stage starts after the "|"
    }
  }

  if (n < 2) { fprintf(stderr, "syntax error near '|'\n"); return; }

  // 2. Create N-1 pipes: pipes[i] connects stage i (write) to stage i+1 (read)
  int pipes[MAX_STAGES-1][2];
  for (int i = 0; i < n-1; i++) {
    if (pipe(pipes[i]) != 0) {
      perror("pipe");
      for (int j = 0; j < i; j++) { close(pipes[j][0]); close(pipes[j][1]); }
      return;
    }
  }

  // 3. Fork one child per stage
  pid_t pids[MAX_STAGES];
  for (int i = 0; i < n; i++) {
    char **argv = stages[i];
    if (argv[0] == NULL) { fprintf(stderr, "syntax error near '|'\n"); break; }

    pids[i] = fork();
    if (pids[i] < 0) { perror("fork"); break; }

    if (pids[i] == 0) {
      // Redirect stdin from previous pipe (all stages except the first)
      if (i > 0) {
        dup2(pipes[i-1][0], STDIN_FILENO);
      }
      // Redirect stdout to next pipe (all stages except the last)
      if (i < n-1) {
        dup2(pipes[i][1], STDOUT_FILENO);
      }

      // Close all pipe fds in this child — only the dup'd ones matter now
      for (int j = 0; j < n-1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }

      run_child(argv, is_builtin(argv[0]));
      _exit(1);
    }
  }

  // 4. Parent: close every pipe fd so children see EOF correctly
  for (int i = 0; i < n-1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  // 5. Wait for all children
  for (int i = 0; i < n; i++) {
    if (pids[i] > 0) { waitpid(pids[i], NULL, 0); }
  }
}
