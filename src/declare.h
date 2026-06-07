#ifndef DECLARE_H
#define DECLARE_H

const char *get_shell_var(const char *name);
void builtin_declare(int argc, char *argv[]);

#endif
