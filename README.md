# Unix Shell from Scratch in C++

A small educational Unix-like shell written in C++17. It launches external programs using `fork()` and `execvp()`, supports pipelines, input/output redirection, background commands, `cd`, and in-session command history.

> **Status:** Learning/portfolio project, not a full POSIX shell. The parser intentionally supports a simple whitespace-separated syntax; quoting, variable expansion, globbing, job control, and shell scripting are not implemented.

## Features

- Execute external commands with `fork()` and `execvp()`
- Pipelines such as `ls | grep cpp | wc -l`
- Input redirection with `<`
- Output redirection with `>` and `>>`
- Background commands with `&`
- Built-in `cd`, `history`, and `exit`
- Reap background child processes to avoid zombies
- CMake build

## Requirements

- Linux (Ubuntu works)
- C++17 compiler such as `g++`
- CMake 3.16 or newer

This project uses Unix process and file-descriptor APIs. Use Ubuntu/WSL2 or another Unix-like system.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/minishell
```

## Examples

```text
minishell$ pwd
minishell$ ls
minishell$ ls | grep cpp | wc -l
minishell$ cat < README.md
minishell$ echo hello > output.txt
minishell$ echo again >> output.txt
minishell$ sleep 5 &
minishell$ history
minishell$ cd ..
minishell$ exit
```

Use spaces around operators for the current parser, for example `echo hello > output.txt`. Values with spaces and quoted arguments are not parsed as a single argument.

## Architecture

1. Read a command line from standard input.
2. Parse simple pipe and redirection operators.
3. For external commands, create child processes using `fork()`.
4. Connect pipe file descriptors with `dup2()`.
5. Replace each child process with the requested program using `execvp()`.
6. Wait for foreground commands, or return to the prompt for background commands.
7. Use a `SIGCHLD` handler to reap completed background children.

## Limitations

- No quote handling, escaping, environment-variable expansion, globbing, command substitution, or semicolon-separated commands.
- Basic background execution only; no process groups, terminal job control, `jobs`, `fg`, or `bg`.
- Redirection is designed for simple commands; advanced combinations may behave differently from Bash.
- History is kept only in memory and is lost when the shell exits.
- This is not a secure sandbox; commands run with the current user's permissions.

## Roadmap

- [ ] Add quote-aware tokenization
- [ ] Save command history between sessions
- [ ] Implement process groups and job control
- [ ] Add automated tests for pipes and redirection
- [ ] Improve parser error reporting

## License

MIT. See [LICENSE](LICENSE).
