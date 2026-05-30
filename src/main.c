#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 128



static const char *builtins[] = {
  "exit",
  "echo",
  "type",
  "pwd",
  "cd",
};
static const size_t builtin_count = sizeof(builtins) / sizeof(builtins[0]);



// AUX UNIVERSAL ===================================
// Check if is Builtin Cmd
bool is_builtin(const char *cmd) {
  if (cmd == NULL) { return false; }

  for (size_t i = 0; i < builtin_count; i++) {
    if (strcmp(cmd, builtins[i]) == 0) { return true; }
  }
  return false;
}


// Find Executable in PATH
// use in: builtin_type
char *find_executable_in_path(const char *cmd) {
  if (cmd == NULL) { return NULL; }
  // PATH is env var that tells shell where to look for executable prog: It's a colon-separated list of dirs to search
  const char *path_env = getenv("PATH");
  if (path_env == NULL) { return NULL; }

  // Copy PATH str to create a writabel copy before tokenizing (bc strtok modifies it)
  char *path_copy = strdup(path_env);
  if (path_copy == NULL) { return NULL; }

  char *dir = strtok(path_copy, ":"); // first dir from PATH
  // While there's still dirs to check
  while (dir != NULL) {
    // 1. Alloc a str for full path
    size_t path_len = strlen(dir) + strlen(cmd) + 2; // EX. "/usr/bin" + "/" + "ls" + "\0"
    char *full_path = malloc(path_len);
    if (full_path == NULL) {
      free(path_copy);
      return NULL;
    }

    // 2. Build actual path
    snprintf(full_path, path_len, "%s/%s", dir, cmd); // snprintf(dest, size, "format str", values, ...);

    // 3. Path exists and is executable
    if (access(full_path, X_OK) == 0) {
      free(path_copy);
      return full_path;
    }
    free(full_path);

    dir = strtok(NULL, ":"); // move on to next dir from PATH
  }
  // Nothing was found
  free(path_copy);
  return NULL;
}


// Tokenize: Turns IN line to ARC/ARGV Form
int build_argv(char *input, char *argv[]) {
  int argc = 0; // arg we count

  char *token = strtok(input, " "); // first token
  while (token != NULL && argc < MAX_ARGS-1) {
    argv[argc] = token;
    argc++;
    token = strtok(NULL, " ");
  }

  argv[argc] = NULL;
  return argc;
}



// AUX FOR BUILTINS ===================================
// ECHO ----
// builtin_echo
void builtin_echo(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    if (i > 1) { printf(" "); }
    printf("%s", argv[i]);
  }

  printf("\n");
}

// TYPE ----
// builtin_type
void builtin_type(int argc, char *argv[]) {
  if (argc < 2) {
    printf("type: missing argument\n");
    return;
  }

  const char *cmd = argv[1];
  if (is_builtin(cmd)) {
    printf("%s is a shell builtin\n", cmd);
    return;
  }

  char *path = find_executable_in_path(cmd);
  if (path != NULL) {
    printf("%s is %s\n", cmd, path);
    free(path);
    return;
  }

  printf("%s: not found\n", cmd);
}

// CD --
// builtin_cd
void builtin_cd(int argc, char *argv[]) {
  if (argc < 2) { return; }

  const char *path = argv[1];

  if (strcmp(argv[1], "~") == 0) {
    const char *home_env = getenv("HOME");
    if (home_env == NULL) { return; }
    path = home_env;
  }

  if (chdir(path) != 0) {
    printf("cd: %s: No such file or directory\n", argv[1]);
  }
}



// EXECUTES ===================================
// Execute a builtin Cmd
void execute_builtin(int argc, char *argv[]) {
  if (strcmp(argv[0], "exit") == 0) {
    exit(0);
  } else if (strcmp(argv[0], "echo") == 0) {
    builtin_echo(argc, argv);
    return;
  } else if (strcmp(argv[0], "type") == 0) {
    builtin_type(argc, argv);
    return;
  } else if (strcmp(argv[0], "pwd") == 0) {
    printf("%s\n", getcwd(NULL, 0));
    return;
  } else if (strcmp(argv[0], "cd") == 0) {
    builtin_cd(argc, argv);
  }
}

// Execute an External Executable
void execute_external_command(const char *path, char *argv[]) {
  // RECALL: fork() called once, ret twice: 0 to child, PID of child to parent
  pid_t pid = fork(); // fork a child proc

  if (pid < 0) {
    perror("fork");
    return;
  } else if (pid == 0) { // inside child proccess (og prog keeps running)
    execv(path, argv); // child becomes the external executable
    // Reached only if execv fails
    perror("execv");
    exit(1);
  }

  // The shell process stays alive and waits for the child prog
  waitpid(pid, NULL, 0);
}





// MAIN ===================================
// Decides whether a Command is Builtin, Executable, or missing
void execute_command(int argc, char *argv[]) {
  if (argc == 0 || argv[0] == NULL) { return; }

  // (a) Builtin cmd
  if (is_builtin(argv[0])) {
    execute_builtin(argc, argv);
    return;
  }

  // (b) External cmd
  char *path = find_executable_in_path(argv[0]);
  if (path == NULL) {
    printf("%s: command not found\n", argv[0]);
    return;
  }

  execute_external_command(path, argv);

  free(path);
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
