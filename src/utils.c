#include "utils.h"
#include "completion.h"
#include "jobs.h"
#include "history.h"
#include "declare.h"
#include "boss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static const char *builtins[] = {
  "exit",
  "echo",
  "type",
  "pwd",
  "cd",
  "complete",
  "jobs",
  "history",
  "declare",
  "boss",
  "unboss",
};
static const size_t builtin_count = sizeof(builtins) / sizeof(builtins[0]);


// AUX HISTORY ===================================
char *history[MAX_HISTORY];
int history_size = 0;

// AUX BACKGROUND JOBS ===================================
Job job_table[MAX_JOBS];
int job_count = 0;

// Check all background jobs, print and remove any that have exited normally
void reap_background_jobs(void) {
  // Pass 1: mark exited jobs as Done
  for (int i = 0; i < job_count; i++) {
    int status;
    pid_t result = waitpid(job_table[i].pid, &status, WNOHANG);
    if (result > 0 && WIFEXITED(status)) { job_table[i].status = "Done"; }
  }

  // Pass 2: print Done jobs (cmd without trailing " &")
  for (int i = 0; i < job_count; i++) {
    if (strcmp(job_table[i].status, "Done") != 0) { continue; }
    char marker = (i == job_count-1) ? '+' : (i == job_count-2) ? '-' : ' ';
    char *cmd = job_table[i].cmd;
    size_t cmdlen = strlen(cmd);
    if (cmdlen >= 2 && strcmp(cmd + cmdlen - 2, " &") == 0) {
      printf("[%d]%c  %-24s%.*s\n", job_table[i].job_num, marker, "Done", (int)(cmdlen - 2), cmd);
    } else {
      printf("[%d]%c  %-24s%s\n", job_table[i].job_num, marker, "Done", cmd);
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


// Find smallest job number not currently in use
static int next_available_job_num(void) {
  for (int candidate = 1; ; candidate++) {
    bool taken = false;
    for (int i = 0; i < job_count; i++) {
      if (job_table[i].job_num == candidate) { taken = true; break; }
    }
    if (!taken) { return candidate; }
  }
}


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


// Tokenize: Turns input line into argc/argv form
// UNDERSTAND: *w is just rewriting input, which is then just argv[] but we have indices of 0...argc-1 that indicate the start of each token (all NULL-terminated)
// - Handles single quotes (preserves spaces, strips quote chars) and concatenation
int build_argv(char *input, char *argv[]) {
  int argc = 0; // count args
  char *p = input;

  while (*p != '\0' && argc < MAX_ARGS-1) {
    while (*p == ' ') { p++; } // skip inter-token whitespace
    if (*p == '\0') break;

    // Write token in-place (read ptr p, write ptr w)
    char *token_start = p;
    char *w = p; // w: token we're manually writing to
    bool in_s_quote = false;
    bool in_d_quote = false;
    bool in_backslash = false;

    while (*p != '\0') {
      // Double Quote Token
      if (in_d_quote) {
        if (in_backslash) { // backslash escapes these within d-quotes: ", \, $, `, and \n
          // escapables
          if (*p == '"' || *p == '\\' || *p == '$' || *p == '`' || *p == '\n') { *w++ = *p++; }
          // non-escapables (just write the backslash and the following char)
          else { *w++ = '\\'; *w++ = *p++; }
          in_backslash = false;
        } 
        else if (*p == '\\')  { in_backslash = true; p++; } 
        else if (*p == '"')   { in_d_quote = false; p++; } 
        else                  { *w++ = *p++; }
      
      // Single Quote Token
      } else if (in_s_quote) {
        if (*p == '\'') {  in_s_quote = false;  p++; } 
        else {  *w++ = *p++; }
      
      // Backslash: next char is always literal
      } else if (in_backslash && !in_s_quote) {
        *w++ = *p++;
        in_backslash = false;
      
      // Space-deliminated Token
      } else {
        if (*p == '\"') { in_d_quote = true; p++; } 
        else if (*p == '\'') { in_s_quote = true; p++; } 
        else if (*p == '\\') { in_backslash = true; p++; } 
        else if (*p == ' ') { break; }
        else { *w++ = *p++; }
      }
    }

    // ATP: *p is either a space delim or \0
    bool was_space = (*p == ' '); // was p the end of a whitespace-token
    *w = '\0'; // null-terminate the token
    if (was_space) { p++; } // skip the space delimiter (to get to start of next token)

    argv[argc++] = token_start; // store and then increment argc
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
    printf("%s", argv[i]); // RECALL: argv[i] is just from i to NULL (\0)
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
    const char *histfile = getenv("HISTFILE");
    if (histfile) { save_history_to_file(histfile); }
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
  } else if (strcmp(argv[0], "complete") == 0) {
    builtin_complete(argc, argv);
  } else if (strcmp(argv[0], "jobs") == 0) {
    builtin_jobs();
    return;
  } else if (strcmp(argv[0], "history") == 0) {
    builtin_history(argc, argv);
  } else if (strcmp(argv[0], "declare") == 0) {
    builtin_declare(argc, argv);
  } else if (strcmp(argv[0], "boss") == 0) {
    builtin_boss();
  } else if (strcmp(argv[0], "unboss") == 0) {
    builtin_unboss();
  }
}

// Execute an External Executable
void execute_external_command(const char *path, char *argv[], bool background) {
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

  if (background) {
    // Build "cmd arg1 arg2 &" string for jobs display
    size_t len = 2; // for " &"
    for (int i = 0; argv[i]; i++) { 
        len += strlen(argv[i]) + 1;
    }
    char *cmd = malloc(len+1);
    cmd[0] = '\0';
    for (int i = 0; argv[i]; i++) {
      if (i > 0) strcat(cmd, " ");
      strcat(cmd, argv[i]);
    }
    strcat(cmd, " &");

    int job_num = next_available_job_num();
    job_table[job_count++] = (Job){ job_num, pid, cmd, "Running" };
    printf("[%d] %d\n", job_num, pid);
  } else {
    // The shell process stays alive and waits for the child prog
    waitpid(pid, NULL, 0);
  }
}



// REDIRECTION ===================================
// Apply Redirection
Redirect apply_redirection(char *args[], int *argc) {
  Redirect r = { .saved_stdout = -1, .saved_stderr = -1 }; // init to assume no redirects found

  int i = 0; // read pos
  int out = 0; // write pos
  while (i < *argc) {
    // NOTE: fd is File Descriptor (int in the OS used to track open file/stream)
    // 3 built-in: 0 stdin, 1 stdout, 2 stderr
    // Open a file, OS gives new int (that the fd)
    int target_fd = -1; // which built-in stream we're redirecting (1 or 2)
    int flags = O_WRONLY | O_CREAT; // edit to tell either to overwrite (O_TRUNC) or append (O_APPEND)

    // REDIRECT TYPES
    if (strcmp(args[i], ">")   == 0 || strcmp(args[i], "1>")  == 0) {
      target_fd = STDOUT_FILENO;
      flags |= O_TRUNC;
    }
    else if (strcmp(args[i], ">>")  == 0 || strcmp(args[i], "1>>") == 0) {
      target_fd = STDOUT_FILENO;
      flags |= O_APPEND;
    }
    else if (strcmp(args[i], "2>")  == 0) {
      target_fd = STDERR_FILENO;
      flags |= O_TRUNC;
    }
    else if (strcmp(args[i], "2>>") == 0) {
      target_fd = STDERR_FILENO;
      flags |= O_APPEND;
    }
    
    // (a) Found a redirect operator, open file (next token: args[i+1]). 0644: file perm (ownder R/W, others R)
    if (target_fd != -1) {
      // NOT good handling (just move on from missing filename and scan rest of tokens for other redirects)
      if (args[i+1] == NULL) {
        fprintf(stderr, "missing filename\n");
        i++;
        continue;
      }
      
      // source we want to point the stream at
      int fd = open(args[i+1], flags, 0644); // RECALL: int open(const char *pathname, int flags, mode_t mode);
      if (fd < 0) {
        // NOT good handling (just skip this redirection)
        perror("open");
        i += 2;
        continue;
      }

      if (target_fd == STDOUT_FILENO && r.saved_stdout == -1) { r.saved_stdout = dup(STDOUT_FILENO); } // note: dup() duplicates the curr fd into a new fd number, saving so we can restore later
      if (target_fd == STDERR_FILENO && r.saved_stderr == -1) { r.saved_stderr = dup(STDERR_FILENO); }

      dup2(fd, target_fd); // dup2(src, dst): makes dst pt to whatever src pointers to
      close(fd); // close extra file fd (still open via stdout/stderr)
      i += 2;
    } 
    // (b) Not a redirect
    else {
      args[out++] = args[i++]; // keep token as is and incr read/write pters
    }
  }
  args[out] = NULL;
  *argc = out;
  return r;
}
// Point stdout/stderr back to terminal, close the saved fd
void restore_redirection(Redirect r) {
  if (r.saved_stdout != -1) {
    dup2(r.saved_stdout, STDOUT_FILENO);
    close(r.saved_stdout); 
  }
  if (r.saved_stderr != -1) {
    dup2(r.saved_stderr, STDERR_FILENO);
    close(r.saved_stderr);
  }
}
