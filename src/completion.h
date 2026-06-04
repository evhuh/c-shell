// Command, Filename, and Programmable Compeltion

#ifndef COMPLETION_H
#define COMPLETION_H

#include <readline/readline.h>

char **shell_completer(const char *text, int start, int end);
void init_completion(void);

#endif