#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



// MAIN ===================================
// Decides whether a Command is Builtin, Executable, or missing
void execute_command(int argc, char *argv[]) {
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
      execute_external_command(path, argv);
      free(path);
    }
  }

  restore_redirection(r);
}


int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while (1) { // 4. LOOP
    // 1. READ
    printf("$ ");

    // 2. EXTRACT
    char input[MAX_INPUT_SIZE];
    if (fgets(input, sizeof(input), stdin) == NULL) { break; } // fgets(buffer, max_size, input_source);
    input[strcspn(input, "\n")] = '\0'; // Remove trailing \n:   strcspn(input, "\n") finds index of where "\n" sits

    // Tokenize
    char *argv[MAX_ARGS];
    int argc = build_argv(input, argv);

    execute_command(argc, argv);
  }

  return 0;
}
