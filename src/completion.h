// Command, Filename, and Programmable Compeltion

#ifndef COMPLETION_H
#define COMPLETION_H

#include <stdio.h>
#include <readline/readline.h>

const char *find_completion(const char *cmd);
void init_completion(void);
void builtin_complete(int argc, char *argv[]);

#endif