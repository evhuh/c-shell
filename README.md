# c-shell

## Overview

`c-shell` is a POSIX-style Unix shell built from scratch in C. It implements the core read-eval-print loop that every shell runs on without relying on any shell framework (tokenizing raw input, resolving commands, forking processes, and wiring up file descriptors).

The project was built as a deep-dive into systems programming: process lifecycle, file descriptor manipulation, signal propagation, and the mechanics that underpin every terminal session.

## Features

### Built-in Commands

| Command | Description |
|---|---|
| `cd [dir\|~]` | Change directory; `~` expands to `$HOME` |
| `echo [args...]` | Print arguments to stdout |
| `exit` | Exit the shell (saves history first) |
| `type <cmd>` | Report whether a command is a builtin or show its PATH location |
| `pwd` | Print the current working directory |
| `jobs` | List active and recently completed background jobs |
| `history [n]` | Print the last `n` history entries (or all if omitted) |
| `history -r <file>` | Read entries from a file into history |
| `history -a <file>` | Append new entries since the last `-a` to a file |
| `history -w <file>` | Write all history entries to a file |
| `declare NAME=VALUE` | Set a shell variable |
| `declare -p NAME` | Inspect a shell variable's current value |
| `complete -C <script> <cmd>` | Register a completer script for a command |
| `complete -r <cmd>` | Remove a registered completer |
| `complete -p <cmd>` | Inspect a registered completer |

### External Command Execution

Commands not recognized as builtins are looked up by walking `$PATH` in order. The shell forks a child process and uses `execv` to replace it with the target executable. If no executable is found, an error is printed to stderr.

### I/O Redirection

| Operator | Effect |
|---|---|
| `> file` / `1> file` | Redirect stdout to file (overwrite) |
| `>> file` / `1>> file` | Redirect stdout to file (append) |
| `2> file` | Redirect stderr to file (overwrite) |
| `2>> file` | Redirect stderr to file (append) |

Redirection is applied by saving the original file descriptor with `dup`, pointing it to the target file with `dup2`, and restoring it after the command completes. Multiple redirections in a single command are supported.

### Pipelines

The shell supports N-stage pipelines using the `|` operator. A flat token array is split on `|` tokens into per-stage argument lists. N-1 pipes are created, and one child process is forked per stage. Each child's stdin and stdout are wired to the appropriate pipe ends via `dup2` before `execv`. The parent closes all pipe ends and waits for every child to exit.

```
$ ls | grep .c | wc -l
```

Pipelines and I/O redirection are mutually exclusive at parse time. Redirection is applied per-command inside `execute_command`, while pipeline stages are dispatched through the dedicated `pipeline()` function.

### Background Jobs

Appending `&` to any external command runs it in the background. The shell prints the job number and PID, then returns the prompt immediately. Background jobs are tracked in a job table and reaped asynchronously before each new prompt. Running `jobs` at any time shows the current job list with status (`Running` / `Done`).

```
$ sleep 10 &
[1] 48231
$ jobs
[1]+  Running                 sleep 10 &
```

### Tab Completion

Tab completion is built on GNU Readline and works in two modes:

**Command completion:** Pressing Tab at the start of a line searches all builtins and every executable reachable through `$PATH`, deduplicates results, sorts them alphabetically, and presents them inline.

**Programmable completion:** After registering a completer script with `complete -C`, pressing Tab mid-command forks that script and captures its stdout line by line as completion candidates. The script receives the command name, the word being completed, the preceding word, and the `COMP_LINE`/`COMP_POINT` environment variables, mirroring the Bash programmable completion interface.

### Command History

History is stored in memory and persists across sessions via `$HISTFILE`. On startup the shell reads `$HISTFILE` into both its internal buffer and Readline's history list, enabling up-arrow recall of past sessions. On exit (or `exit` command) the full history is written back. The `history` builtin provides fine-grained control over reading, appending, and writing history to arbitrary files.

### Quoting and Escaping

The tokenizer (`build_argv`) handles:
- **Single quotes:** Everything between `'...'` is treated literally; no special characters inside.
- **Double quotes:** Most characters are literal, but `\`, `"`, `$`, `` ` ``, and `\n` can be escaped with a backslash.
- **Backslash:** Outside quotes, escapes the next character unconditionally.
- **Token concatenation:** Adjacent quoted and unquoted segments are joined into a single token.

### Shell Variables and Expansion

Shell variables are declared with `declare NAME=VALUE` and expanded anywhere in a command with `$NAME` or `${NAME}`. Expansion happens after tokenization and before execution. Tokens that expand to the empty string are dropped from the argument list.

```
$ declare GREETING=hello
$ echo $GREETING world
hello world
$ echo ${GREETING}_there
hello_there
```

## Architecture

```
src/
├── main.c       — REPL loop: read, tokenize, expand variables, dispatch
├── utils.c      — tokenizer, builtin implementations, PATH lookup,
│                  I/O redirection, external process execution, job tracking
├── pipeline.c   — N-stage pipeline: split on |, create pipes, fork children
├── completion.c — Readline integration, command/filename completion,
│                  programmable completion registry and runner
├── history.c    — history buffer management, file read/write operations
├── jobs.c       — background job table, jobs builtin
└── declare.c    — shell variable store, declare builtin, $VAR expansion
```

**REPL flow:**

1. **Read** `readline()` captures a line, feeding it through Readline's line-editing and completion hooks before returning it.
2. **Tokenize** `build_argv()` walks the raw string in a single pass, handling quotes and backslashes in-place.
3. **Expand** each token is scanned for `$` and variable values are substituted.
4. **Dispatch** if a `|` token is present the input goes to `pipeline()`; otherwise `execute_command()` handles builtin vs. external lookup, redirection, and optional backgrounding.
5. **Print** command output goes directly to stdout (or to a redirected file descriptor). Background job notifications and completions print before the next prompt.

## Getting Started

### Prerequisites

- GCC
- GNU Readline

```bash
# macOS
brew install readline

# Ubuntu / Debian
sudo apt install libreadline-dev
```

### Build

```bash
make build
```

Compiles all source files under `src/` with `-Wall -Wextra -std=c11` and places the binary at `build/shell`.

### Run

```bash
make run # build + launch
./build/shell # or directly
```

To enable persistent history, set `HISTFILE` in your environment before launching:

```bash
HISTFILE=~/.c_shell_history ./build/shell
```

### Clean

```bash
make clean
```

## Usage & Examples

```bash
# Basic commands
$ echo "hello, world"
hello, world

$ pwd
/Users/eva/projects/c-shell

$ cd ~/Desktop && pwd
/Users/eva/Desktop

# Type resolution
$ type echo
echo is a shell builtin
$ type ls
ls is /bin/ls

# I/O redirection
$ echo "log entry" >> output.log
$ ls nonexistent 2> errors.txt

# Pipelines
$ cat /etc/hosts | grep localhost | wc -l
1

$ ls -la | grep "^d" | sort

# Background jobs
$ sleep 5 &
[1] 91234
$ jobs
[1]+  Running                 sleep 5 &

# Shell variables
$ declare VERSION=1.0
$ echo "Running v$VERSION"
Running v1.0

# History
$ history 5
    8  ls -la
    9  cd src
   10  make build
   11  ./build/shell
   12  history 5

$ history -w ~/.c_shell_history

# Tab completion
$ ec<TAB>          # completes to "echo"
$ git <TAB>        # runs registered completer script for git
```

## What I Learned

Building the shell from scratch covered a wide range of systems programming fundamentals:

- **Process lifecycle:** `fork()`, `exec()`, `waitpid()`, and the difference between `_exit()` in children and `exit()` in the parent.
- **File descriptors:** Tthe OS tracks open files as integers; saving and restoring them with `dup`/`dup2` for redirection; creating anonymous byte channels between processes with `pipe()`.
- **Pipelines:** Chaining N commands requires N-1 pipes, N forks, careful fd wiring, and closing every unused pipe end in every process so EOF propagates correctly.
- **Readline integration:** Hooking into GNU Readline's completion and display callbacks; understanding the generator/iterator pattern it uses to collect matches.
- **Quoting semantics:** The subtle differences between single-quoted, double-quoted, and bare tokens. The tokenizer needs to track nested state to get them right.
- **Background jobs:** Non-blocking process reaping with `WNOHANG`; maintaining a job table with stable job numbers across exits.
- **Variable expansion:** Implementing a pre-execution pass that substitutes `$VAR` and `${VAR}` references inline, dropping empty-expansion tokens.
- **Memory management:** Careful `malloc`/`free` discipline when `readline` heap-allocates input, when `strdup` copies tokens, and when the job table owns command strings.

## Future Work

- **Input redirection** (`< file`): Feed a file as stdin to a command.
- **Here-documents** (`<< EOF`): Inline multiline input blocks.
- **Command substitution** (`$(cmd)`): Embed a command's output as an argument.
- **Arithmetic expansion** (`$((expr))`): Evaluate integer expressions inline.
- **Signal handling**: Proper `SIGINT`/`SIGTSTP` management so Ctrl-C kills the foreground child without killing the shell, and Ctrl-Z suspends it.
- **`fg` / `bg` builtins**: Resume suspended jobs in the foreground or background.
- **`if` / `while` / `for`**: Basic control flow constructs.
