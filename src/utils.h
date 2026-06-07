// Navigation, Quoting, Redirection
// Completion

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <sys/types.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 128


#define MAX_JOBS 64
typedef struct { int job_num; pid_t pid; char *cmd; char *status; } Job;
extern Job job_table[MAX_JOBS];
extern int job_count;


void reap_background_jobs(void);

bool is_builtin(const char *cmd);
const char *find_completion(const char *cmd);
char *find_executable_in_path(const char *cmd);
int build_argv(char *input, char *argv[]);
void builtin_echo(int argc, char *argv[]);
void builtin_type(int argc, char *argv[]);
void builtin_cd(int argc, char *argv[]);
void execute_builtin(int argc, char *argv[]);
void execute_external_command(const char *path, char *argv[], bool background);
void execute_command(int argc, char *argv[], bool background);

// File Descriptors (saved og of stdout and stderr before we redirect them)
typedef struct {
  int saved_stdout; // -1 if not redirected
  int saved_stderr; // -1 if not redirected
} Redirect;

void restore_redirection(Redirect r);
Redirect apply_redirection(char *args[], int *argc);

#endif