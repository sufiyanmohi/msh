#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include<stdbool.h>
#include <fcntl.h>
#include<ctype.h>
#define readLine_buffer 512
#define tokens_buffer_size 32
#define token_delimiters " \t\r\n\a"
#define redirection_buffer_size 4
#define sequence_buffer_size 4
#define piping_buffer_size 4
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
char * trim(char * s){
    while(isspace((unsigned char)*s))s++;
    if(*s=='\0')return s;
    char * end = s + strlen(s) -1;
    while(end > s && isspace((unsigned char)*end))end--;
    if(s==end)s[0]='\0';
    else end[1]='\0';
    return s;
}
char ** msh_tokenizeLineLayerOne(char *line, char ** op_ptr){
    char ** sequences = malloc(sequence_buffer_size*sizeof(char *));
    char * op = malloc(sequence_buffer_size*sizeof(char));
    int size = sequence_buffer_size;
    int sequence_index =0;
    if (!sequences || !op)
    {
        fprintf(stderr, "msh : allocation error\n");
        exit(EXIT_FAILURE);
    }
    int line_index = 0 ;
    char * token = &line[line_index];
    while(line[line_index]!='\0'){
        if(line[line_index]=='|' && line[line_index+1]=='|'){
            line[line_index]='\0';
            line[line_index+1]='\0';
            sequences[sequence_index]=trim(token);
            op[sequence_index]='O';
            sequence_index++;
            line_index+=2;
            token=&line[line_index];
        }
        else if(line[line_index]=='&' && line[line_index+1]=='&'){
            line[line_index]='\0';
            line[line_index+1]='\0';
            sequences[sequence_index]=trim(token);
            op[sequence_index]='A';
            sequence_index++;
            line_index+=2;
            token=&line[line_index];
        }
        else if(line[line_index]=='&'){
            line[line_index]='\0';
            sequences[sequence_index]=trim(token);
            op[sequence_index]='B';
            sequence_index++;
            line_index++;
            token=&line[line_index];
        }
        else line_index++;
        if (sequence_index >= size)
        {
            size += sequence_buffer_size;
            sequences = realloc(sequences, size * sizeof(char *));
            op = realloc(op, size * sizeof(char));
            if (!sequences || !op)
            {
                fprintf(stderr, "msh : allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    op[sequence_index]='\0';
    sequences[sequence_index++]=trim(token);
    sequences[sequence_index]=NULL;
    *op_ptr = op;
    return sequences;
}
char ** msh_tokenizeLineLayerTwo(char * line , int * number_of_pipes){
    char ** piping = malloc(piping_buffer_size*sizeof(char *));
    int size = piping_buffer_size;
    int piping_index =0;
    if (!piping)
    {
        fprintf(stderr, "msh : allocation error\n");
        exit(EXIT_FAILURE);
    }

    int i=0;
    char * token_p = &line[i];
    while(line[i]!='\0'){
        if(line[i]=='|'){
            line[i]='\0';
            piping[piping_index++]=trim(token_p);
            if(piping_index>=size){
                size+=piping_buffer_size;
                piping=realloc(piping,size*sizeof(char*));
                if (!piping)
                {
                    fprintf(stderr, "msh : allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
            token_p=&line[i+1];
        }
        i++;
    }
    piping[piping_index++]=trim(token_p);
    piping[piping_index]=NULL;
    *number_of_pipes = piping_index-1;
    return piping;
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
void masked_exec(char * line){
    int * redirection =NULL;
    char ** args = msh_tokenizeLine(line,&redirection);
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
    free(redirection);
    if (execvp(args[0], args) == -1)
    {
        perror(args[0]);
    }
    exit(EXIT_FAILURE);
    
}
int msh_executePipeArgs(char ** piping,int number_of_pipes){
    if(piping[0]==NULL){
        return 1;
    }
    if(piping[number_of_pipes]==NULL || *piping[number_of_pipes]=='\0'){
        fprintf(stderr,"msh : expected file after | \n");
        return 1;
    }
    int fds[number_of_pipes][2];
    int status;
    pid_t pids[number_of_pipes+1];
    for(int i=0;i<number_of_pipes;i++){
        pipe(fds[i]);
    }
    for(int i=0;i<=number_of_pipes;i++){
        pids[i]=fork();
        if (pids[i] < 0)
        {
            fprintf(stderr, "msh : Fork failed \n");
            exit(EXIT_FAILURE);
        }
        if(pids[i]==0){
            if(i<number_of_pipes){
                dup2(fds[i][1],STDOUT_FILENO);
                close(fds[i][1]);
            }
            if(i>0){
                dup2(fds[i-1][0],STDIN_FILENO);
                close(fds[i-1][0]);
            }
            for(int j=0;j<number_of_pipes;j++){
                close(fds[j][0]);
                close(fds[j][1]);
            }
            masked_exec(piping[i]);
            exit(EXIT_FAILURE);
        }
    }
    for(int i=0;i<number_of_pipes;i++){
        close(fds[i][0]);
        close(fds[i][1]);
    }
    for(int i=0;i<number_of_pipes;i++){
        waitpid(pids[i],NULL,0);
    }
    do
    {
        waitpid(pids[number_of_pipes], &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status) == 0;   
    }
    else
    {
        return 0;   
    }
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
            return 0;
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
            if (WIFEXITED(status))
            {
                return WEXITSTATUS(status) == 0;   
            }
            else
            {
                return 0;   
            }
        }
    }
    return 1;
}
int msh_executeLayerOne(char ** sequences,char * op){
    int status;
    int i=0;
    while(sequences[i]){
        char * line = sequences[i];
        char ** pipe_args;
        int number_of_pipes;
        pipe_args =  msh_tokenizeLineLayerTwo(line,&number_of_pipes);
        if(number_of_pipes){
            status = msh_executePipeArgs(pipe_args,number_of_pipes);
        }
        else{
            char **args;
            int *redirection=NULL;
            args = msh_tokenizeLine(line,&redirection);
            status = msh_executeLine(args,redirection);
            free(args);
            free(redirection);
        }
        free(pipe_args);
        if(op[i]=='A'){
            if(!status)return 1;
        }
        else if(op[i]=='O'){
            if(status)return 1;
        }
        i++;
    }
    return 1;
}
int main()
{
    char *line;
    char ** sequences;
    int status;
    do
    {
        char * op;
        line = msh_readLine();
        sequences = msh_tokenizeLineLayerOne(line,&op);
        status = msh_executeLayerOne(sequences,op);
        free (op);
        free(line);
        free(sequences);
        
    } while (status);
    return 0;
}
/*
gcc helloincd -o helloincd
op : ld: unsupported mach-o filetype (only MH_OBJECT and MH_DYLIB can be linked) in '/Users/sufiyanmohiuddin/Desktop/sufiyan_coding/shell_in_c/cd_hello/helloincd'
clang: error: linker command failed with exit code 1 (use -v to see invocation)
./hello || ./helloincd  
op : ./hello: No such file or directory
op2 : ./hello: No such file or directory
hello this is helloincd.c 
false | true && echo "pipeline succeeded"
"pipeline succeeded"
> true | false && echo "should NOT print"
"should NOT print"
> true | false || echo "pipeline failed"
> false | false | true && echo "still succeeds"
"still succeeds"
> ./nonexistent | echo hi
./nonexistent: No such file or directory
hi
*/