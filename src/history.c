#include "history.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/history.h>

int history_last_saved = 0;

void save_history_to_file(const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) { fprintf(stderr, "history: %s: cannot open file\n", path); return; }
  for (int i = 0; i < history_size; i++) {
    fprintf(f, "%s\n", history[i]);
  }
  fclose(f);
}

// Builtin handler for the "history" command
void builtin_history(int argc, char *argv[]) {
  // history -r <file>: append file contents to history
  if (argc >= 3 && strcmp(argv[1], "-r") == 0) {
    FILE *f = fopen(argv[2], "r");
    if (!f) { fprintf(stderr, "history: %s: cannot open file\n", argv[2]); return; }
    char line[MAX_INPUT_SIZE];
    while (fgets(line, sizeof(line), f)) {
      // Strip trailing newline
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') { line[--len] = '\0'; }
      if (len == 0) { continue; } // skip empty lines
      if (history_size < MAX_HISTORY) {
        history[history_size++] = strdup(line);
        add_history(line); // also feed readline for up-arrow recall
      }
    }
    fclose(f);
    return;
  }

  // history -a <file>: append only new entries (since last -a) to file
  if (argc >= 3 && strcmp(argv[1], "-a") == 0) {
    FILE *f = fopen(argv[2], "a");
    if (!f) { fprintf(stderr, "history: %s: cannot open file\n", argv[2]); return; }
    for (int i = history_last_saved; i < history_size; i++) {
      fprintf(f, "%s\n", history[i]);
    }
    fclose(f);
    history_last_saved = history_size;
    return;
  }

  // history -w <file>: write all history entries to file
  if (argc >= 3 && strcmp(argv[1], "-w") == 0) {
    FILE *f = fopen(argv[2], "w");
    if (!f) { fprintf(stderr, "history: %s: cannot open file\n", argv[2]); return; }
    for (int i = 0; i < history_size; i++) {
      fprintf(f, "%s\n", history[i]);
    }
    fclose(f);
    return;
  }

  // history [n]: print last n entries (or all if no n)
  int start = 0;
  if (argc >= 2) {
    int n = atoi(argv[1]);
    if (n > 0 && n < history_size) { start = history_size - n; }
  }
  for (int i = start; i < history_size; i++) {
    printf("    %d  %s\n", i + 1, history[i]);
  }
}
