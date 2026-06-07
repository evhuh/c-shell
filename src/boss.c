#include "boss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define SAFE_URL "https://yale.instructure.com/courses/94011" // REPLACE W WHATEVER IS TO BE PULLED UP
#define BOSS_PROMPT "\001\033[32m\002[studying]\001\033[0m\002 \001\033[34m\002>>\001\033[0m\002 "
#define DEFAULT_PROMPT "$ "

// Game process name patterns to match against (pgrep -f style: substring of full argv)
static const char *game_patterns[] = { // ADD/REPLACE W THINGS
  // "Minecraft Launcher",
  "RobloxPlayer",
  "RobloxCrashHandler",
  "RobloxStudio",
  NULL,
};

// PIDs we paused, so unboss sends SIGCONT to exactly the same processes
#define MAX_PAUSED 64
static pid_t paused_pids[MAX_PAUSED];
static int paused_count = 0;

static int boss_active = 0;

// Returns the prompt string readline should display
const char *get_prompt(void) {
  return boss_active ? BOSS_PROMPT : DEFAULT_PROMPT;
}

// Run: pgrep -fi <pattern>  and return newline-separated PIDs via popen
// prints 1 PID per line: read line-by-line, convert to an int with atoi, then call kill(pid, SIGSTOP), SIGSTOP freeze process
static void pause_by_pattern(const char *pattern) {
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "pgrep -fi '%s' 2>/dev/null", pattern);

  FILE *fp = popen(cmd, "r"); // fork, execv, and pipe
  if (!fp) return;

  char line[32];
  while (fgets(line, sizeof(line), fp)) {
    pid_t pid = (pid_t)atoi(line);
    if (pid <= 0) continue;
    if (paused_count >= MAX_PAUSED) break;

    if (kill(pid, SIGSTOP) == 0) { // SIGSTOP: freeze proc
      paused_pids[paused_count++] = pid;
      printf("  [boss] paused PID %d (%s)\n", pid, pattern);
    }
  }
  pclose(fp);
}


// Boss
void builtin_boss(void) {
  if (boss_active) {
    printf("boss: already active\n");
    return;
  }

  printf("going studious\n");

  // Open Good APP: fork + exec "open <url>" detached
  pid_t pid = fork();
  if (pid == 0) {
    // child: become "open <url>"
    char *open_argv[] = { "open", SAFE_URL, NULL };
    execvp("open", open_argv);
    _exit(1);
  }

  // Pause games
  paused_count = 0;
  for (int i = 0; game_patterns[i] != NULL; i++) {
    pause_by_pattern(game_patterns[i]);
  }
  if (paused_count == 0) {
    printf("  [boss] no game processes found\n");
  }

  boss_active = 1;
}


// Unboss
void builtin_unboss(void) {
  if (!boss_active) {
    printf("unboss: boss mode not active\n");
    return;
  }

  printf("back to fun\n");

  // Resume every PID we paused (send SIGCONT)
  for (int i = 0; i < paused_count; i++) {
    if (kill(paused_pids[i], SIGCONT) == 0) {
      printf("  [unboss] resumed PID %d\n", paused_pids[i]);
    }
  }
  paused_count = 0;

  boss_active = 0;
}
