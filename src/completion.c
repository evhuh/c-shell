// CMD COMPLETION FLOW: matches[] --> command_generator (collects them one at a time) --> rl_completion_matches (collects all internally) --> Readline (displays)

#include "completion.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <readline/readline.h>

static const char *builtins[] = {
    "echo",
    "exit",
    "type",
    "pwd",
    "cd",
    NULL};

// Pre-collected match list (rebuilt each time the user presses TAB on a new prefix)
static int match_count = 0;
static char **matches = NULL;
// Free every str in the arr and resets match count and match arr
static void clear_matches(void)
{
    for (int i = 0; i < match_count; i++)
    {
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
    for (int i = 0; builtins[i]; i++)
    {
        if (strncmp(builtins[i], text, len) == 0)
        {
            matches = realloc(matches, sizeof(char *) * (match_count + 1));
            matches[match_count++] = strdup(builtins[i]);
        }
    }

    // 2. PATH executables
    const char *path_env = getenv("PATH");
    if (!path_env)
    {
        return;
    }

    char *path_copy = strdup(path_env);
    char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        DIR *d = opendir(dir);
        if (!d) {
            dir = strtok(NULL, ":"); // advance to next PATH entry (even on failure)
            continue; // skip nonexistent dirs
        }

        // Read Entries in Directory and add matches
        struct dirent *ent; // single dir(entry)
        while ((ent = readdir(d))) { // readdir(d) : rets next entry in the dir one at a time until NULL
            if (strncmp(ent->d_name, text, len) != 0)
            {
                continue;
            } // ent->d_name : to access name of entry

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


// 2. Router: called once per TAB press
static char **shell_completer(const char *text, int start, int end) {
    (void)end;

    if (start != 0) { return NULL; } // only complete the first word
    // rl's match collector calls command_generator repeatedly until gets NULL
    return rl_completion_matches(text, command_generator);
}


// 3. Setup: runs once at startup
void init_completion(void) {
    rl_attempted_completion_function = shell_completer; // register shell_compelter as the fun rl calls every TAB press
    rl_completion_append_character = '  '; // append a space after a completed word
}
