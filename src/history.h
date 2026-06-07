// Shell history management

#ifndef HISTORY_H
#define HISTORY_H

extern int history_last_saved;
void builtin_history(int argc, char *argv[]);
void save_history_to_file(const char *path);

#endif
