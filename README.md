# MSH

A simple shell implementation in C, inspired by Stephen Brennan's tutorial, extended with I/O redirection, piping,command sequencing and background processing.

Provides built-in commands `cd` , `exit` and `jobs`, and uses a combination of `fork()` and `execvp()` to execute other commands like `ls`, `echo`, `gcc`, `python3`, etc., along with their arguments.

## Features

- **Built-ins**: `cd`, `exit` , `jobs`
- **External commands**: anything on `$PATH`, via `fork()` + `execvp()`
- **I/O redirection**: `>` (truncate), `>>` (append), `<` (input)
- **Piping**: multi-stage pipelines, e.g. `ls | grep msh | wc -l`
- **Command sequencing**: `&&`, `||`  operators e.g. `make && ./msh || ./hello`
- **Background Process**: `&` operator to run process in bg e.g. `./msh &`

## How it works

The shell runs in a loop with several stages:

1. **`msh_readLine()`** — reads a line of input using `getline()`
2. **`msh_tokenizeLineLayerOne()`** — splits the line into sequences by `&&`, `||`, and `&`
3. **`msh_tokenizeLineLayerTwo()`** — splits each sequence into pipeline stages by `|`
4. **`msh_tokenizeLine()`** — splits a single command into arguments and redirection targets
5. **`msh_executeLine()` / `msh_executePipeArgs()`** — runs the command(s) by 
    1.  Calling `chdir()` in parent for `cd`
    2.  Creating process groups and handing Terminal Control to child:`fork()` + `setpgid()` + `tcsetpgrp()` + `signal(SIGxxxx,SIG_DFL)` + `execvp()` 
    3.  And `dup2()` for redirection and `pipe()` + `dup2()` for piping
6. **`msh_executeLayerOne()`** — evaluates sequencing operators and background process by 
    1. Checking exit status using `WEXITSTATUS` for `&&` and `||`
    2. Creating process groups (no TC):`fork()` + `setpgid()` + `signal(SIGxxxx,SIG_DFL)` + `execvp()` 
## Build & Run

```bash
gcc msh.c -o msh
./msh
```

## Example usage
```bash
ls -la | grep msh
echo hello > out.txt
cat out.txt >> log.txt
make && ./msh || ./hello
false || echo "fallback"
./hello & sleep 5 &
jobs
kill <pid>
exit
```
## Credits

1. Tutorial – [Write a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) by Stephen Brennan
2. Claude for debugging assistance
3. APUE - Advanced Programming in the UNIX Environment, Third Edition (Chapter 8,9,10)
