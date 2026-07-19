MSH
Simple implementation of a shell in c , inspired by Stephen Brennan's tutorial .
Provides inbuilt commands cd , exit and uses combination of fork() and execvp() to execute other commands like ls , echo , gcc , python3 etc.. paired with other required arguments . 
The entire code execution follows a loop : 
ssh_readLine() using getline()
ssh_tokenizeLine() using strtok()
ssh_executeLine() using chdir() for cd and combination of fork() and execvp() for other commands
Credits:
1. Tutorial - [Write a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) by Stephen Brennan
2. Claude for debugging assistance