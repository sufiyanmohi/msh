#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include<stdbool.h>
#include <fcntl.h>
#define readLine_buffer 512
#define tokens_buffer_size 32
#define token_delimiters " \t\r\n\a"
#define redirection_buffer_size 4
char *msh_readLine()
{
    char *line = NULL;
    size_t buffer_size = 0;
    printf("> ");
    ssize_t len = getline(&line, &buffer_size, stdin);
    if (len == -1)
    {
        exit(EXIT_SUCCESS);
    }
    if (len > 0 && line[len - 1] == '\n')
    {
        line[len - 1] = '\0';
    }
    return line;
}
char **msh_tokenizeLine(char *line,int ** redirection_ptr)
{
    char **tokens = malloc(tokens_buffer_size * sizeof(char *));
    int * redirection = malloc(redirection_buffer_size * sizeof(int));
    int redirection_index =0;
    int redirection_size=redirection_buffer_size;
    int buffer_size = tokens_buffer_size;
    char *token;
    int position = 0;
    if (!tokens || !redirection)
    {
        fprintf(stderr, "msh : allocation error\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, token_delimiters);
    while (token != NULL)
    {
        tokens[position] = token;
        if(position > 0 && ( token[0] == '>' || token[0] == '<') ){
            redirection[redirection_index]=position;
            redirection_index++;
            if(redirection_index>=redirection_buffer_size){
                redirection_size += redirection_buffer_size;
                redirection=realloc(redirection,redirection_size*sizeof(int));
                if (!redirection)
                {
                    fprintf(stderr, "msh : allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        position++;
        if (position >= buffer_size)
        {
            buffer_size += tokens_buffer_size;
            tokens = realloc(tokens, buffer_size * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "msh : allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, token_delimiters);
    }
    tokens[position] = NULL;
    redirection[redirection_index]=-1;
    *redirection_ptr = redirection;
    return tokens;
}
int msh_executeLine(char **args,int * redirection)
{   
    if (args[0] == NULL)
    {
        return 1;
    }   
    if (strcmp(args[0], "cd") == 0)
    {
        if (args[1] == NULL)
        {
            fprintf(stderr, "msh: expected argument to \"cd\"\n");
        }
        else if (chdir(args[1]) != 0)
        {
            perror(args[1]);
        }
        return 1;
    }
    else if (strcmp(args[0], "exit") == 0)
    {
        return 0;
    }
    else
    {
        int status;
        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "msh : Fork failed \n");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            if(redirection!=NULL){
                int i =0 ;
                while(redirection[i]!=-1){
                    if(args[redirection[i]+1]!=NULL){
                        if(strcmp(args[redirection[i]],">")==0){
                            int file = open(args[redirection[i]+1], O_WRONLY | O_CREAT | O_TRUNC , 0644);
                            if(file==-1){
                                fprintf(stderr, "msh : unable to open %s \n",args[redirection[i]+1]);
                                exit(EXIT_FAILURE);
                            }
                            dup2(file,STDOUT_FILENO);
                            close(file);  
                        }
                        else if(strcmp(args[redirection[i]],">>")==0){
                                int file = open(args[redirection[i]+1], O_WRONLY | O_CREAT | O_APPEND , 0644);
                                if(file==-1){
                                    fprintf(stderr, "msh : unable to open %s \n",args[redirection[i]+1]);
                                    exit(EXIT_FAILURE);
                                }
                                dup2(file,STDOUT_FILENO);
                                close(file);
                                
                        }
                        else if(args[redirection[i]][0]=='<'){
                                int file = open(args[redirection[i]+1], O_RDONLY, 0644);
                                if(file==-1){
                                    fprintf(stderr, "msh : unable to open %s \n",args[redirection[i]+1]);
                                    exit(EXIT_FAILURE);
                                }
                                dup2(file,STDIN_FILENO);
                                close(file);
                                
                        }
                    }
                    else {
                        fprintf(stderr, "msh : expected file after '>'\n");
                        exit(EXIT_FAILURE);
                    }
                    i++;
                }
                args[redirection[0]]=NULL;
            }
            
            if (execvp(args[0], args) == -1)
            {
                perror(args[0]);
            }
            exit(EXIT_FAILURE);
        }
        else
        {
            do
            {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
    return 1;
}
int main()
{
    char *line;
    char **args;
    int status;
    do
    {
        int *redirection=NULL;
        line = msh_readLine();
        args = msh_tokenizeLine(line,&redirection);
        status = msh_executeLine(args,redirection);
        free(redirection);
        free(line);
        free(args);
    } while (status);
    return 0;
}