# MSH

A simple shell implementation in C, inspired by Stephen Brennan's tutorial.

Provides built-in commands `cd` and `exit`, and uses a combination of `fork()` and `execvp()` to execute other commands like `ls`, `echo`, `gcc`, `python3`, etc., along with their arguments.

## How it works

The shell runs in a loop with three stages:

1. **`msh_readLine()`** — reads a line of input using `getline()`
2. **`msh_tokenizeLine()`** — splits the line into arguments using `strtok()`
3. **`msh_executeLine()`** — runs the command: uses `chdir()` for `cd`, or `fork()` + `execvp()` for everything else

## Build & Run

```bash
gcc msh.c -o msh
./msh
```

## Credits

1. Tutorial – [Write a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) by Stephen Brennan
2. Claude for debugging assistance
