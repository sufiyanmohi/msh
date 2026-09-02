#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#define readLine_buffer 512
#define tokens_buffer_size 32
#define token_delimiters " \t\r\n\a"
#define redirection_buffer_size 4
#define sequence_buffer_size 4
#define piping_buffer_size 4
static int should_exit = 0;
sigset_t mask, oldmask;
int msh_terminal ;
int job_counter = 0;
struct job_node {
    int job_id;
    //pid_t pid ;
    pid_t pgid ;
    enum status {
        running = 1,
        suspended = 0,
        terminated = -1,
    }status;
    char * cmd;
    struct job_node * next ;
}*head=NULL;
void print_jobs(struct job_node * node){
    while(node){
        if(node->status == 1){
            if(node->next){
                printf("[%d] - running %s\n",node->job_id,node->cmd);
            }
            else printf("[%d] + running %s\n",node->job_id,node->cmd);
        }
        else  if (node->status == 0){
            if(node->next){
                printf("[%d] - suspended %s\n",node->job_id,node->cmd);
            }
            else printf("[%d] + suspended %s\n",node->job_id,node->cmd);
        }
        else {
            if(node->next){
                printf("[%d] - terminated %s\n",node->job_id,node->cmd);
            }
            else printf("[%d] + terminated %s\n",node->job_id,node->cmd);
        }
        node=node->next;
    }
} 
struct job_node * add_job(struct job_node * head,int job_id,int status, char * cmd,pid_t pgid){
    if(!head){
        head = (struct job_node *)malloc(sizeof(*head));
        head->next = NULL;
        head->job_id= job_id;
        head->status = status;
        head->cmd = strdup(cmd);
        head->pgid = pgid;
        return head;
    }
    struct job_node * ptr = head;
    while (ptr->next)
    {
    ptr=ptr->next;
    }
    struct job_node * new_node = (struct job_node *)malloc(sizeof(*new_node));
    new_node->next = NULL;
    new_node->job_id= job_id;
    new_node->status = status;
    new_node->cmd = strdup(cmd);
    new_node->pgid=pgid;
    ptr->next = new_node;
    return head;
}
struct job_node * remove_job(struct job_node * head , pid_t pgid){
    struct job_node * ptr = head ;
    while( ptr && ptr->pgid != pgid){
        ptr = ptr->next;
    }
    if(ptr){
        if(ptr==head) {
            struct job_node * temp = head ;
            head = head->next;
            free(temp->cmd);
            free(temp);
            return head;
        }
        struct job_node * temp = head;
        while(temp->next!=ptr){
            temp = temp->next;
        }
        temp->next=ptr->next;
        free(ptr->cmd);
        free(ptr);
    }
    return head;
}
char *msh_readLine()
{
    char *line = NULL;
    size_t buffer_size = 0;
    printf("> ");
    ssize_t len;
    while ((len = getline(&line, &buffer_size, stdin)) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        exit(EXIT_SUCCESS);
    }
    if (len > 0 && line[len - 1] == '\n')
    {
        line[len - 1] = '\0';
    }
    return line;
}
char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    if (s == end)
        s[0] = '\0';
    else
        end[1] = '\0';
    return s;
}
char **msh_tokenizeLineLayerOne(char *line, char **op_ptr)
{
    char **sequences = malloc(sequence_buffer_size * sizeof(char *));
    char *op = malloc(sequence_buffer_size * sizeof(char));
    int size = sequence_buffer_size;
    int sequence_index = 0;
    if (!sequences || !op)
    {
        fprintf(stderr, "msh : allocation error\n");
        exit(EXIT_FAILURE);
    }
    int line_index = 0;
    char *token = &line[line_index];
    while (line[line_index] != '\0')
    {
        if (line[line_index] == '|' && line[line_index + 1] == '|')
        {
            line[line_index] = '\0';
            line[line_index + 1] = '\0';
            sequences[sequence_index] = trim(token);
            op[sequence_index] = 'O';
            sequence_index++;
            line_index += 2;
            token = &line[line_index];
        }
        else if (line[line_index] == '&' && line[line_index + 1] == '&')
        {
            line[line_index] = '\0';
            line[line_index + 1] = '\0';
            sequences[sequence_index] = trim(token);
            op[sequence_index] = 'A';
            sequence_index++;
            line_index += 2;
            token = &line[line_index];
        }
        else if (line[line_index] == '&')
        {
            line[line_index] = '\0';
            sequences[sequence_index] = trim(token);
            op[sequence_index] = 'B';
            sequence_index++;
            line_index++;
            token = &line[line_index];
        }
        else
            line_index++;
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
    op[sequence_index] = '\0';
    sequences[sequence_index++] = trim(token);
    sequences[sequence_index] = NULL;
    *op_ptr = op;
    return sequences;
}
char **msh_tokenizeLineLayerTwo(char *line, int *number_of_pipes)
{
    char **piping = malloc(piping_buffer_size * sizeof(char *));
    int size = piping_buffer_size;
    int piping_index = 0;
    if (!piping)
    {
        fprintf(stderr, "msh : allocation error\n");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    char *token_p = &line[i];
    while (line[i] != '\0')
    {
        if (line[i] == '|')
        {
            line[i] = '\0';
            piping[piping_index++] = trim(token_p);
            if (piping_index >= size)
            {
                size += piping_buffer_size;
                piping = realloc(piping, size * sizeof(char *));
                if (!piping)
                {
                    fprintf(stderr, "msh : allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
            token_p = &line[i + 1];
        }
        i++;
    }
    piping[piping_index++] = trim(token_p);
    piping[piping_index] = NULL;
    *number_of_pipes = piping_index - 1;
    return piping;
}
char **msh_tokenizeLine(char *line, int **redirection_ptr)
{
    char **tokens = malloc(tokens_buffer_size * sizeof(char *));
    int *redirection = malloc(redirection_buffer_size * sizeof(int));
    int redirection_index = 0;
    int redirection_size = redirection_buffer_size;
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
        if (position > 0 && (token[0] == '>' || token[0] == '<'))
        {
            redirection[redirection_index] = position;
            redirection_index++;
            if (redirection_index >= redirection_buffer_size)
            {
                redirection_size += redirection_buffer_size;
                redirection = realloc(redirection, redirection_size * sizeof(int));
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
    redirection[redirection_index] = -1;
    *redirection_ptr = redirection;
    return tokens;
}
void signal_reset(){
    signal(SIGTTOU,SIG_DFL);
    signal(SIGTTIN,SIG_DFL);
    signal(SIGTSTP,SIG_DFL);
    signal(SIGINT,SIG_DFL);
}
struct job_node * find_job_jid(struct job_node * head , int job_id){
    struct job_node * ptr = head;
    while(ptr && ptr->job_id!=job_id){
        ptr=ptr->next;
    }
    return ptr;
}
struct job_node * find_job_pid(struct job_node * head , pid_t pid){
    struct job_node * ptr = head;
    while(ptr && ptr->pgid!=pid){
        ptr=ptr->next;
    }
    return ptr;
}
void fg_helper(struct job_node * ptr){
    int status;
    pid_t fg_pgid = ptr->pgid;
    int fg_jid = ptr->job_id;
    enum status fg_status = ptr->status;
    char * fg_cmd =strdup(ptr->cmd) ;
    struct job_node * fg_next = ptr->next;
    if(ptr->status==suspended || ptr->status == running){
        if(ptr->status==running){
            if(ptr->next){
                printf("[%d] - running %s\n",ptr->job_id,ptr->cmd);
            }
            else printf("[%d] + running %s\n",ptr->job_id,ptr->cmd);
        }
        if(tcsetpgrp(msh_terminal,ptr->pgid)==0){
            head=remove_job(head,ptr->pgid);
            if(fg_status==suspended){
                if(fg_next){
                    printf("[%d] - continued %s\n",fg_jid,fg_cmd);
                }
                else printf("[%d] + continued %s\n",fg_jid,fg_cmd);
                if(kill(-fg_pgid,SIGCONT)==-1){
                    fprintf(stderr,"msh : fg failed !\n");
                }
            }
            do
            {
                waitpid(-fg_pgid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status) );//
            tcsetpgrp(msh_terminal,getpgrp());
            if(WIFSTOPPED(status)){
                job_counter++;
                head = add_job(head,job_counter,suspended,fg_cmd,fg_pgid);
            }
        }
        else fprintf(stderr,"msh : fg failed !\n");
    }
    free(fg_cmd);
}
void bg_helper(struct job_node * ptr){
    if(ptr->status==suspended){
        ptr->status=running;
        if(ptr->next){
            printf("[%d] - continued %s\n",ptr->job_id,ptr->cmd);
        }
        else printf("[%d] + continued %s\n",ptr->job_id,ptr->cmd);
        if(kill(-ptr->pgid,SIGCONT)==-1){
            fprintf(stderr,"msh : bg failed !\n");
        }
    }
}
void masked_exec(char *line)
{
    int *redirection = NULL;
    char **args = msh_tokenizeLine(line, &redirection);
    if (redirection != NULL)
    {
        int i = 0;
        while (redirection[i] != -1)
        {

            if (args[redirection[i] + 1] != NULL)
            {
                if (strcmp(args[redirection[i]], ">") == 0)
                {
                    int file = open(args[redirection[i] + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (file == -1)
                    {
                        fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                        exit(EXIT_FAILURE);
                    }

                    dup2(file, STDOUT_FILENO);
                    close(file);
                }
                else if (strcmp(args[redirection[i]], ">>") == 0)
                {
                    int file = open(args[redirection[i] + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (file == -1)
                    {
                        fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                        exit(EXIT_FAILURE);
                    }
                    dup2(file, STDOUT_FILENO);
                    close(file);
                }
                else if (args[redirection[i]][0] == '<')
                {
                    int file = open(args[redirection[i] + 1], O_RDONLY, 0644);
                    if (file == -1)
                    {
                        fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                        exit(EXIT_FAILURE);
                    }
                    dup2(file, STDIN_FILENO);
                    close(file);
                }
            }
            else
            {
                fprintf(stderr, "msh : expected file after '>'\n");
                exit(EXIT_FAILURE);
            }
            i++;
        }
        args[redirection[0]] = NULL;
    }
    free(redirection);
    sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
    signal_reset();
    if (execvp(args[0], args) == -1)
    {
        perror(args[0]);
    }
    exit(EXIT_FAILURE);
}
int msh_executePipeArgs(char **piping, int number_of_pipes)
{
    for (int i = 0; i <= number_of_pipes; i++)
    {
        if (piping[i] == NULL || *piping[i] == '\0')
        {
            fprintf(stderr, "msh : expected files with | \n");
            return 1;
        }
    }
    int fds[number_of_pipes][2];
    int status;
    pid_t pids[number_of_pipes + 1];
    for (int i = 0; i < number_of_pipes; i++)
    {
        pipe(fds[i]);
    }
    for (int i = 0; i <= number_of_pipes; i++)
    {
        pids[i] = fork();
        
        if (pids[i] < 0)
        {
            fprintf(stderr, "msh : Fork failed \n");
            exit(EXIT_FAILURE);
        }
        if (pids[i] == 0)
        {
            if(i==0){
                setpgid(0,0);
                tcsetpgrp(msh_terminal,pids[0]);
            }
            else setpgid(pids[i],pids[0]);
            if (i < number_of_pipes)
            {
                dup2(fds[i][1], STDOUT_FILENO);
                close(fds[i][1]);
            }
            if (i > 0)
            {
                dup2(fds[i - 1][0], STDIN_FILENO);
                close(fds[i - 1][0]);
            }
            for (int j = 0; j < number_of_pipes; j++)
            {
                close(fds[j][0]);
                close(fds[j][1]);
            }

            masked_exec(piping[i]);
            exit(EXIT_FAILURE);
        }
    }
    setpgid(pids[0],pids[0]);
    tcsetpgrp(msh_terminal,pids[0]);
    for(int i=1;i<=number_of_pipes;i++){
        setpgid(pids[i],pids[0]);
    }
    for (int i = 0; i < number_of_pipes; i++)
    {   
        close(fds[i][0]);
        close(fds[i][1]);
    }
    for (int i = 0; i < number_of_pipes; i++)
    {
        waitpid(pids[i], NULL, 0);
    }
    do
    {
        waitpid(pids[number_of_pipes], &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    tcsetpgrp(msh_terminal,getpgrp());
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status) == 0;
    }
    else
    {
        return 0;
    }
}
int msh_executeLine(char **args, int *redirection)
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
    else if(strcmp(args[0],"jobs")==0){
        print_jobs(head);
        return 1;
    }
    else if (strcmp(args[0],"fg")==0){
        if (!args[1]){
            if (!head) {
                fprintf(stderr, "msh: fg: no current job\n");
                return 1;
            }
            struct job_node * ptr = head;
            while(ptr->next)ptr=ptr->next;
            fg_helper(ptr);
        }
        else if (args[1][0]=='%'){
            char * endptr ;
            long jid = strtol((args[1]+1),&endptr,10);
            if(*(args[1]+1)=='\0' || *endptr != '\0'){
                fprintf(stderr,"msh : %%%ld no such job id\n",jid);
                return 1;
            }
            struct job_node * ptr = find_job_jid(head,(int)jid);
            if (!ptr) {
                fprintf(stderr, "msh: fg: %%%ld: no such job\n", jid);
                return 1;
            }
            fg_helper(ptr);
        }
        return 1;
    }
    else if (strcmp(args[0],"bg")==0){
        if(!args[1]){
            if(!head){
                fprintf(stderr, "msh: fg: no current job\n");
                return 1;
            }
            struct job_node * ptr = head;
            while(ptr->next)ptr=ptr->next;
            bg_helper(ptr);
        }
        else if(args[1][0]=='%'){
            char * endptr;
            long jid = strtol(args[1]+1,&endptr,10);
            if(*(args[1]+1)=='\0' || *endptr != '\0'){
                fprintf(stderr,"msh : %%%ld no such job id\n",jid);
                return 1;
            }
            struct job_node * ptr =find_job_jid(head,jid);
            if(!ptr){
                fprintf(stderr, "msh: bg: %%%ld: no such job\n", jid);
                return 1;
            }
            bg_helper(ptr);
        }
        return 1;
    }
    else if (strcmp(args[0],"kill")==0 && args[1] && args[1][0]=='%'){
        char * endptr;
        long jid = strtol(args[1]+1,&endptr,10);
        if(*(args[1]+1)=='\0' || *endptr != '\0'){
            fprintf(stderr,"msh : %%%ld no such job id\n",jid);
            return 1;
        }
        struct job_node * ptr =find_job_jid(head,jid);
        if(!ptr){
            fprintf(stderr, "msh: bg: %%%ld: no such job\n", jid);
            return 1;
        }
        if(ptr->status==running || ptr->status==suspended){
            kill(-ptr->pgid,SIGINT);//i hope sigchld handler will take care
        }
        return 1;
    }
    else if (strcmp(args[0], "exit") == 0)
    {
        should_exit = 1;
        return 1;
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
            setpgid(0,0);
            tcsetpgrp(msh_terminal,getpid());
            if (redirection != NULL)
            {
                int i = 0;
                while (redirection[i] != -1)
                {
                    if (args[redirection[i] + 1] != NULL)
                    {
                        if (strcmp(args[redirection[i]], ">") == 0)
                        {
                            int file = open(args[redirection[i] + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if (file == -1)
                            {
                                fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                                exit(EXIT_FAILURE);
                            }
                            dup2(file, STDOUT_FILENO);
                            close(file);
                        }
                        else if (strcmp(args[redirection[i]], ">>") == 0)
                        {
                            int file = open(args[redirection[i] + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                            if (file == -1)
                            {
                                fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                                exit(EXIT_FAILURE);
                            }
                            dup2(file, STDOUT_FILENO);
                            close(file);
                        }
                        else if (args[redirection[i]][0] == '<')
                        {
                            int file = open(args[redirection[i] + 1], O_RDONLY, 0644);
                            if (file == -1)
                            {
                                fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                                exit(EXIT_FAILURE);
                            }
                            dup2(file, STDIN_FILENO);
                            close(file);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "msh : expected file after '>'\n");
                        exit(EXIT_FAILURE);
                    }
                    i++;
                }
                args[redirection[0]] = NULL;
            }
            sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
            signal_reset();
            if (execvp(args[0], args) == -1)
            {
                perror(args[0]);
            }
            exit(EXIT_FAILURE);
        }
        else
        {
            setpgid(pid,pid);
            tcsetpgrp(msh_terminal,pid);
            do
            {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

            tcsetpgrp(msh_terminal,getpgrp());
            if (WIFEXITED(status))
            {
                return WEXITSTATUS(status) == 0;
            }
            else if(WIFSTOPPED(status)){
                job_counter++;
                head = add_job(head,job_counter,suspended,(char *)args[0],pid);
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
    return 1;
}
int msh_executeLayerOne(char **sequences, char *op)
{
    sigemptyset(&mask);
    sigemptyset(&oldmask);
    sigaddset(&mask, SIGCHLD);
    int status = 1;
    int i = 0;
    while (sequences[i])
    {
        char *line = sequences[i];
        char **pipe_args;
        int number_of_pipes;
        pipe_args = msh_tokenizeLineLayerTwo(line, &number_of_pipes);
        if (number_of_pipes)
        {
            sigprocmask(SIG_BLOCK, &mask, &oldmask);
            status = msh_executePipeArgs(pipe_args, number_of_pipes);
            sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
        }
        else if (op[i] == 'B')
        {
            char **args;
            int *redirection = NULL;
            args = msh_tokenizeLine(line, &redirection);
            int status;
            job_counter++;
            sigprocmask(SIG_BLOCK, &mask, &oldmask);//
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "msh : Fork failed \n");
                exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            {
                printf("[%d] pid %d\n", job_counter,getpid());
                setpgid(0,0);
                if (redirection != NULL)
                {
                    int i = 0;
                    while (redirection[i] != -1)
                    {
                        if (args[redirection[i] + 1] != NULL)
                        {
                            if (strcmp(args[redirection[i]], ">") == 0)
                            {
                                int file = open(args[redirection[i] + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                if (file == -1)
                                {
                                    fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                                    exit(EXIT_FAILURE);
                                }
                                dup2(file, STDOUT_FILENO);
                                close(file);
                            }
                            else if (strcmp(args[redirection[i]], ">>") == 0)
                            {
                                int file = open(args[redirection[i] + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                                if (file == -1)
                                {
                                    fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                                    exit(EXIT_FAILURE);
                                }
                                dup2(file, STDOUT_FILENO);
                                close(file);
                            }
                            else if (args[redirection[i]][0] == '<')
                            {
                                int file = open(args[redirection[i] + 1], O_RDONLY, 0644);
                                if (file == -1)
                                {
                                    fprintf(stderr, "msh : unable to open %s \n", args[redirection[i] + 1]);
                                    exit(EXIT_FAILURE);
                                }
                                dup2(file, STDIN_FILENO);
                                close(file);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "msh : expected file after '>'\n");
                            exit(EXIT_FAILURE);
                        }
                        i++;
                    }
                    args[redirection[0]] = NULL;
                }
                signal_reset();
                if (execvp(args[0], args) == -1)
                {
                    perror(args[0]);
                }
                exit(EXIT_FAILURE);
            }
            else
            {
                setpgid(pid,pid);
                head = add_job(head,job_counter,running,(char *)args[0],pid);
                sigprocmask(SIG_UNBLOCK, &mask, &oldmask);//
                free(args);
                free(redirection);
                status = 1;
            }
        }
        else
        {
            sigprocmask(SIG_BLOCK, &mask, &oldmask);
            char **args;
            int *redirection = NULL;
            args = msh_tokenizeLine(line, &redirection);
            status = msh_executeLine(args, redirection);
            free(args);
            free(redirection);
            sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
        }
        free(pipe_args);
        if (op[i] == 'A')
        {
            if (!status)
                return 1;
        }
        else if (op[i] == 'O')
        {
            if (status)
                return 1;
        }
        i++;
    }
    return status;
}
void sigchld_handler(int sig)
{
    int saved_errno = errno;
    int status;
    pid_t p;
    while ((p = waitpid(-1, &status, WNOHANG)) > 0)
    {
        struct job_node * temp = find_job_pid(head,p);
        if(temp){
            if(temp->next){
            printf("[%d] - terminated %s\n",temp->job_id,temp->cmd);
        }
        else printf("[%d] + terminated %s\n",temp->job_id,temp->cmd);
        head = remove_job(head,p);
        }
    }
    errno = saved_errno;
}
int main()
{
    char *line;
    char **sequences;
    int status;
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGINT,SIG_IGN);
    msh_terminal = open("/dev/tty",O_RDWR);
    do
    {
        char *op;
        line = msh_readLine();
        sequences = msh_tokenizeLineLayerOne(line, &op);
        status = msh_executeLayerOne(sequences, op);
        free(op);
        free(line);
        free(sequences);
    } while (!should_exit);
    return 0;
}
