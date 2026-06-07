#include "declare.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_VARS 256

typedef struct { char *name; char *value; } ShellVar;
static ShellVar vars[MAX_VARS];
static int var_count = 0;

// Preproc
// pass over argv after toekenizing, replace any tokens starting with $ w it's var val
const char *get_shell_var(const char *name) {
  for (int i = 0; i < var_count; i++) {
    if (strcmp(vars[i].name, name) == 0) { return vars[i].value; }
  }
  return NULL;
}

static ShellVar *find_var(const char *name) {
  for (int i = 0; i < var_count; i++) {
    if (strcmp(vars[i].name, name) == 0) { return &vars[i]; }
  }
  return NULL;
}

void builtin_declare(int argc, char *argv[]) {
  if (argc >= 3 && strcmp(argv[1], "-p") == 0) {
    ShellVar *v = find_var(argv[2]);
    if (v) { printf("declare -- %s=\"%s\"\n", v->name, v->value); }
    else   { fprintf(stderr, "declare: %s: not found\n", argv[2]); }
    return;
  }

  // declare NAME=VALUE
  if (argc >= 2 && strchr(argv[1], '=') != NULL) {
    char *eq = strchr(argv[1], '=');
    *eq = '\0';
    char *name  = argv[1];
    char *value = eq + 1;

    // Validate identifier: Must start with letter or _, rest alnum or _
    bool valid = (name[0] != '\0') && (isalpha((unsigned char)name[0]) || name[0] == '_');
    for (int i = 1; valid && name[i] != '\0'; i++) {
      if (!isalnum((unsigned char)name[i]) && name[i] != '_') { valid = false; }
    }
    if (!valid) {
      *eq = '='; // restore before printing
      fprintf(stderr, "declare: `%s': not a valid identifier\n", argv[1]);
      return;
    }

    ShellVar *v = find_var(name);
    if (v) {
      free(v->value);
      v->value = strdup(value);
    } else if (var_count < MAX_VARS) {
      vars[var_count++] = (ShellVar){ strdup(name), strdup(value) };
    }
  }
}
