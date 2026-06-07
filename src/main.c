#include "utils.h"
#include "completion.h"
#include "pipeline.h"
#include "history.h"
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

  // [Completion]
  init_completion();

  // [History] Load history from HISTFILE on startup
  const char *histfile = getenv("HISTFILE");
  if (histfile) {
    FILE *f = fopen(histfile, "r");
    if (f) {
      char line[MAX_INPUT_SIZE];
      while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') { line[--len] = '\0'; }
        if (len == 0) { continue; }
        if (history_size < MAX_HISTORY) {
          history[history_size++] = strdup(line);
          add_history(line);
        }
      }
      fclose(f);
    }
  }

  while (1) { // 4. LOOP
    // Reap any completed background jobs before printing prompt
    reap_background_jobs();

    // 1. READ (readline handles/intercepts TAB, arrow keys, etc.)
    char *input = readline("$ ");
    if (input == NULL) { break; } // EOF (Ctrl-D)
    if (input[0] == '\0') { free(input); continue; } // skip blank lines

    add_history(input); // [Completion] makes up-arrow recall work

    // [History]
    if (history_size < MAX_HISTORY) {
      history[history_size++] = strdup(input);
    }

    // EXTRACT — Tokenize
    char *local_argv[MAX_ARGS];
    int local_argc = build_argv(input, local_argv);

    // Detect pipeline operator: scan for any "|" token
    bool has_pipe = false;
    for (int i = 0; i < local_argc; i++) {
      if (strcmp(local_argv[i], "|") == 0) { has_pipe = true; break; }
    }

    if (has_pipe) { // 2. EXECUTE pipeline
      pipeline(local_argv, local_argc);
    } else { // Detect background operator
      bool background = false;
      if (local_argc > 0 && strcmp(local_argv[local_argc - 1], "&") == 0) {
        local_argv[--local_argc] = NULL;
        background = true;
      }

      // 2. EXECUTE + 3. PRINT
      execute_command(local_argc, local_argv, background);
    }

    free(input); // readline malloc's the line — we must free it
  }

  // [History] Write History on Exit
  if (histfile) { save_history_to_file(histfile); }
  // [History] free locally
  for (int i = 0; i < history_size; i++) { free(history[i]); }

  return 0;
}
