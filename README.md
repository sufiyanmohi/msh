# MSH

A simple shell implementation in C, inspired by Stephen Brennan's tutorial, extended with I/O redirection, piping, and command sequencing.

Provides built-in commands `cd` and `exit`, and uses a combination of `fork()` and `execvp()` to execute other commands like `ls`, `echo`, `gcc`, `python3`, etc., along with their arguments.

## Features

- **Built-ins**: `cd`, `exit`
- **External commands**: anything on `$PATH`, via `fork()` + `execvp()`
- **I/O redirection**: `>` (truncate), `>>` (append), `<` (input)
- **Piping**: multi-stage pipelines, e.g. `ls | grep msh | wc -l`
- **Command sequencing**: `&&`, `||`, and `&` operators, e.g. `make && ./msh`
- **Exit status propagation**: real exit codes (via `WIFEXITED`/`WEXITSTATUS`) drive `&&`/`||` chains, not just fork/exec success

## How it works

The shell runs in a loop with several stages:

1. **`msh_readLine()`** — reads a line of input using `getline()`
2. **`msh_tokenizeLineLayerOne()`** — splits the line into sequences by `&&`, `||`, and `&`
3. **`msh_tokenizeLineLayerTwo()`** — splits each sequence into pipeline stages by `|`
4. **`msh_tokenizeLine()`** — splits a single command into arguments and redirection targets
5. **`msh_executeLine()` / `msh_executePipeArgs()`** — runs the command(s): `chdir()` for `cd`, or `fork()` + `execvp()` (with `dup2()` for redirection/piping) for everything else
6. **`msh_executeLayerOne()`** — evaluates sequencing operators, short-circuiting on the propagated exit status

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
make && ./msh
false || echo "fallback"
```
## Credits

1. Tutorial – [Write a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) by Stephen Brennan
2. Claude for debugging assistance
