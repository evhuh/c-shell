/*
CMD & FILENAME COMPLETION FLOW: matches[] --> command_generator (collects them one at a time) --> rl_completion_matches (collects all internally) --> Readline (displays)

- Notes: https://www.gnu.org/software/bash/manual/html_node/Programmable-Completion-Builtins.html
*/

/*
PROGRAMMABLE COMPLETION

user types "git <TAB>"
  shell_completer sees start != 0
    extracts "git" from the line buffer
    finds script registered for "git"

    run_completer(script):
      forks a child
      child execs the script, its stdout goes into the pipe
      parent reads the pipe line by line → populates matches[]

    rl_completion_matches hands matches[] to Readline one at a time

  Readline displays: add  commit  push  status

*/


#include "completion.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <readline/readline.h>

static const char *builtins[] = {
  "echo",
  "exit",
  "type",
  "pwd",
  "cd",
  "complete",
  "jobs",
  NULL
};

// Pre-collected match list (rebuilt each time the user presses TAB on a new prefix)
static int match_count = 0;
static char **matches = NULL;
// Free every str in the arr and resets match count and match arr
static void clear_matches(void)
{
  for (int i = 0; i < match_count; i++) {
    free(matches[i]);
  }
  free(matches);
  matches = NULL;
  match_count = 0;
}

// Collects all matching builtins + PATH executables into matches[], sorted alphabetically
static void collect_matches(const char *text)
{
  clear_matches();
  size_t len = strlen(text);

  // 1. Builtins
  for (int i = 0; builtins[i]; i++) {
    if (strncmp(builtins[i], text, len) == 0) {
      matches = realloc(matches, sizeof(char *) * (match_count + 1));
      matches[match_count++] = strdup(builtins[i]);
    }
  }

  // 2. PATH executables
  const char *path_env = getenv("PATH");
  if (!path_env) { return; }

  char *path_copy = strdup(path_env);
  char *dir = strtok(path_copy, ":");
  while (dir != NULL) {
    DIR *d = opendir(dir);
    if (!d) {
      dir = strtok(NULL, ":"); // advance to next PATH entry (even on failure)
      continue; // skip nonexistent dirs
    }

    // Read Entries in Directory and add matches
    struct dirent *ent; // single dir entry
    while ((ent = readdir(d))) { // readdir(d): rets next entry in the dir one at a time until NULL
      if (strncmp(ent->d_name, text, len) != 0) { continue; } // ent->d_name: to access name of entry

      // Build full path and check if it's executable OK
      char full_path[1024];
      snprintf(full_path, sizeof(full_path), "%s/%s", dir, ent->d_name);
      if (access(full_path, X_OK) != 0) { continue; }

      // Remove Duplicate: Skip if already in matches
      int dup = 0;
      for (int i = 0; i < match_count; i++) {
        if (strcmp(matches[i], ent->d_name) == 0) {
          dup = 1;
          break; // (a) dont add
        }
      }
      if (!dup) { // (b) do add
        matches = realloc(matches, sizeof(char *) * (match_count + 1));
        matches[match_count++] = strdup(ent->d_name);
      }
    }
    closedir(d);
    dir = strtok(NULL, ":"); // advance to next PATH entry
  }
  free(path_copy);

  // Sort array alphabetically in-place
  // qsort(array, array len, sz of ea element, comparator)
  qsort(matches, match_count, sizeof(char *), (int (*)(const void *, const void *))strcmp);
}


// Fork the registered completer script, capture its stdout line by line into matches[]
// argv[1]=cmd  argv[2]=word being completed  argv[3]=word before it
// env: COMP_LINE=full line  COMP_POINT=cursor byte offset
static void run_completer(const char *script, const char *cmd, const char *word, const char *prev_word, const char *comp_line, int comp_point) {
  clear_matches();

  // 1. pipefd(): creates a pipe (a pair of file descriptors where pipefd[1] is WRITE end and pipefd[0] is READ end)
  int pipefd[2]; // pipefd[0]=read end, pipefd[1]=write end
  if (pipe(pipefd) != 0) { return; }

  // 2. Fork the process
  pid_t pid = fork();
  if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return; }

  if (pid == 0) {
    // Child: redirect stdout --> pipe write end, then exec the script
    close(pipefd[0]); // child doesnt need READ end
    dup2(pipefd[1], STDOUT_FILENO); // replace stdout with pipe's WRITE end
    close(pipefd[1]);

    // set env vars on child only (before exec, so they're inherited by the script)
    char comp_point_str[32];
    snprintf(comp_point_str, sizeof(comp_point_str), "%d", comp_point);
    setenv("COMP_LINE", comp_line, 1);
    setenv("COMP_POINT", comp_point_str, 1);

    execl(script, script, cmd, word, prev_word, NULL);
    _exit(1); // exec failed
  }

  // Parent: close write end, read output line by line
  close(pipefd[1]); // parent doesnt need WRITE end
  FILE *f = fdopen(pipefd[0], "r");
  if (!f) { waitpid(pid, NULL, 0); return; }

  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    // Strip trailing newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') { line[--len] = '\0'; }
    if (len == 0) { continue; }
    matches = realloc(matches, sizeof(char *) * (match_count + 1));
    matches[match_count++] = strdup(line);
  }
  fclose(f);
  waitpid(pid, NULL, 0);

  if (match_count > 1) {
    qsort(matches, match_count, sizeof(char *), (int (*)(const void *, const void *))strcmp);
  }
}

// 1. Iterator: Called repeatedly by readline, returns one match per call
static char *command_generator(const char *text, int state) {
  static int index;

  if (state == 0) {
    index = 0;
    collect_matches(text); // rebuild sorted match list once per TAB sequence
  }

  if (index < match_count) { return strdup(matches[index++]); }
  return NULL;
}
// Iterator for Programmable Completion (completer-script) matches (parallel to command_generator)
static char *prog_command_generator(const char *text, int state) {
  (void)text;

  static int index;
  if (state == 0) { index = 0; }
  if (index < match_count) { return strdup(matches[index++]); }
  return NULL;
}


// 2. Router: called once per TAB press
static char **shell_completer(const char *text, int start, int end) {
  (void)end;

  // (a) Completing an Argument: extract the command name from the line buffer
  if (start != 0) {
    char cmd[256];

    // (1) Walk backwd past trailing spaces to find end of first token
    int cmd_len = start; // byte offset of the current word
    while (cmd_len > 0 && rl_line_buffer[cmd_len - 1] == ' ') { cmd_len--; }

    // (2) Find end of first token
    int cmd_start = 0;
    int i = 0;
    while (i < cmd_len && rl_line_buffer[i] != ' ') { i++; }

    // (3) Calculate len of cmd token
    int tok_len = i - cmd_start;
    if (tok_len <= 0 || tok_len >= (int)sizeof(cmd)) { return NULL; }

    // (4) Copy the cmd name into cmd and null-term (ie. cmd = "git")
    strncpy(cmd, rl_line_buffer + cmd_start, tok_len);
    cmd[tok_len] = '\0';

    // (5) Look up if cmd has registered completer (if not, fallback to filename completion)
    const char *script = find_completion(cmd);
    if (!script) { return NULL; }

    // (6) Extract prev_word: token immediately before the current word (between cmd and text)
    char prev_word[256] = ""; {
      int pw_end = start;
      while (pw_end > 0 && rl_line_buffer[pw_end - 1] == ' ') { pw_end--; } // skip spaces before curr word
      int pw_start = pw_end;
      while (pw_start > 0 && rl_line_buffer[pw_start - 1] != ' ') { pw_start--; } // walk back to start of prev token
      int pw_len = pw_end - pw_start;
      if (pw_len > 0 && pw_len < (int)sizeof(prev_word)) {
        strncpy(prev_word, rl_line_buffer + pw_start, pw_len);
        prev_word[pw_len] = '\0';
      }
    }
    int comp_point = start + (int)strlen(text); // cursor = start of current word + chars typed so far
    char comp_line[1024];
    strncpy(comp_line, rl_line_buffer, comp_point);
    comp_line[comp_point] = '\0';

    run_completer(script, cmd, text, prev_word, comp_line, comp_point);

    if (match_count == 0) {
      fputc('\x07', stdout);
      fflush(stdout);
      rl_attempted_completion_over = 1; // tell rl: don't fall back to default filename completion
      return NULL;
    }
    return rl_completion_matches(text, prog_command_generator);

  // (b) Completing a Command (Builtins and filenames)
  } else {
    return rl_completion_matches(text, command_generator);
  }
}


// Display hook: Prints candidates as "cand1  cand2  cand3" then reprints the prompt+input
static void display_matches(char **m, int num, int max_len) {
  (void)max_len;
  printf("\n");
  for (int i = 1; i <= num; i++) { // m[0] is the common prefix, actual matches start at m[1]
    if (i > 1) { printf("  "); }

    struct stat st;
    int is_dir = (stat(m[i], &st) == 0 && S_ISDIR(st.st_mode));
    printf("%s%s", m[i], is_dir ? "/" : "");
  }
  printf("\n");
  rl_on_new_line();
  rl_redisplay();
}

// 3. Setup: runs once at startup
void init_completion(void) {
  rl_attempted_completion_function = shell_completer;
  rl_completion_append_character = ' ';
  rl_completion_display_matches_hook = (void *)display_matches;
}
