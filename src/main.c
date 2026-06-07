#include "utils.h"
#include "completion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>



// MAIN ===================================
// Decides whether a Command is Builtin, Executable, or missing
void execute_command(int argc, char *argv[], bool background) {
  if (argc == 0 || argv[0] == NULL) { return; }

  Redirect r = apply_redirection(argv, &argc);

  // (a) Builtin cmd
  if (is_builtin(argv[0])) {
    execute_builtin(argc, argv);
  // (b) External cmd
  } else {
    char *path = find_executable_in_path(argv[0]);
    if (path == NULL) {
      fprintf(stderr, "%s: command not found\n", argv[0]);
    } else {
      execute_external_command(path, argv, background);
      free(path);
    }
  }

  restore_redirection(r);
}


int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  init_completion();

  while (1) { // 4. LOOP
    // Reap any completed background jobs before printing prompt
    reap_background_jobs();

    // 1. READ (readline handles/intercepts TAB, arrow keys, etc.)
    char *input = readline("$ ");
    if (input == NULL) { break; } // EOF (Ctrl-D)
    if (input[0] == '\0') { free(input); continue; } // skip blank lines

    add_history(input); // [Completion] makes up-arrow recall work

    // EXTRACT — Tokenize
    char *local_argv[MAX_ARGS];
    int local_argc = build_argv(input, local_argv);

    // Detect background operator
    bool background = false;
    if (local_argc > 0 && strcmp(local_argv[local_argc - 1], "&") == 0) {
      local_argv[--local_argc] = NULL;
      background = true;
    }

    // 2. EXECUTE + 3. PRINT
    execute_command(local_argc, local_argv, background);

    free(input); // readline malloc's the line — we must free it
  }

  return 0;
}
