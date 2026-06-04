#include "completion.h"
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

static const char *builtins[] = {
    "echo",
    "exit",
    "type",
    "pwd",
    "cd",
    NULL 
};



// Iterator
// called repeatedly by readline: return the next match for `text` || NULL when done
// state == 0 on the first call for a given completion, then incr
static char *builtin_generator(const char *text, int state) {
  static int i;
  static size_t len;

  if (state == 0) {
    i = 0;
    len = strlen(text);
  }

  // Walk the builtins list and return the next one that starts with `text`
  while (builtins[i] != NULL) {
    const char *name = builtins[i++];
    if (strncmp(name, text, len) == 0) {
      return strdup(name); // readline expects a malloc'd string (it will free it)
    }
  }

  return NULL; // no more matches
}


// Router
// start == 0: check only complete builtins when the cursor is on the 1st word
// start > 0: filename completion
// text : word under the cursor
char **shell_completer(const char *text, int start, int end) {
  // start/end : byte offsets in rl_line_buffer (the full line so far)
  (void)end;

  // 1. Only complete builtins when completing the first word on the line
  if (start != 0) { return NULL; }

  // 2.
  // rl_completion_matches calls builtin_generator repeatedly (state 0, 1, 2, …) and collects all non-NULL results into a NULL-terminated array, text is fallback
  return rl_completion_matches(text, builtin_generator);
}

// Setup
void init_completion(void) {
  rl_attempted_completion_function = shell_completer; // function pointer, readline (rl) calls shell_completer once per TAB
}
