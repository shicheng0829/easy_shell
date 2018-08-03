#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAXARG 8			
#define MAX 1024		
#define MAXCMD 16	

typedef struct command {
    char *argv[MAXARG];		  //ls -l -F -a -R   
    char *in, *out;
} cmd_t;

int parse(char *buf, cmd_t *cmdp)
{
    char *tmp = buf, *curcmd;
    int cmdnum = 0, i = 0, j = 0;
		
    if ((cmdp[cmdnum].argv[0] = strtok(tmp, "|"))) {	  //strsep
		puts(cmdp[cmdnum].argv[0]);					
        cmdnum++;
	}
    while ((cmdp[cmdnum].argv[0] = strtok(NULL, "|"))) {	
        cmdnum++;										
	}
    for (i = 0; i < cmdnum; i++) {		
        curcmd = cmdp[i].argv[0];		
        cmdp[i].in = cmdp[i].out = NULL;

        while (*curcmd) {
            if (*curcmd == ' ') {				
                *curcmd++ = '\0';			
                continue;
            }
            if (*curcmd == '<') {				
                *curcmd = '\0';				
                while (*(++curcmd) == ' ')	
                    ;
                cmdp[i].in = curcmd;		
                if (*curcmd == '\0')
                    return -1;				
                continue;
            }
            if (*curcmd == '>') {				
                *curcmd = '\0';
                while (*(++curcmd) == ' ')
                    ;
                cmdp[i].out = curcmd;
                if (*curcmd == '\0')
                    return -1;
                continue;
            }
            if (*curcmd != ' ' && ((curcmd == cmdp[i].argv[0]) || *(curcmd - 1) == '\0')) {
                cmdp[i].argv[j++] = curcmd++;	
                continue;					
            }
            curcmd++;
        }
        cmdp[i].argv[j] = NULL;
        j = 0;
    }

    return cmdnum;
}

void show(cmd_t *cmdp, int cmdnum)    
{
    int i, j;

    for (i = 0; i < cmdnum; i++) {
        printf("cmd-show:%s,", cmdp[i].argv[0]);

        for(j = 1; cmdp[i].argv[j]; j++)
            printf("argv:%s,", cmdp[i].argv[j]);

        printf("in:%s, out:%s\n", cmdp[i].in, cmdp[i].out);
    }
}

int main(void)
{
    char buf[MAX];		
    pid_t pid;
    int cmdnum, pipenum;
    cmd_t cmd[MAXCMD];   
    int fd, i, j, pfd[MAXCMD][2];

    while (1) {
        printf("sh%%");
        fflush(stdout);				
        fgets(buf, MAX, stdin);

        if (buf[strlen(buf) - 1] == '\n')	
            buf[strlen(buf) - 1] = '\0';
        if (strcasecmp(buf, "exit") == 0 || strcasecmp(buf, "quit") == 0 
           ||strcasecmp(buf, "EXIT") == 0 || strcasecmp(buf, "bye") == 0)	
            break;

        cmdnum = parse(buf, cmd);	
        //show(cmd, cmdnum);		
        pipenum = cmdnum - 1;
        
        //create pipe
        for(i = 0; i < pipenum; i++)
        {
            if(pipe(pfd[i]))
            {
                perror("pipe error");
                exit(1);
            }
        }

        //create process
        for(i = 0; i < cmdnum; i++) 
        {
            pid = fork();
            if(pid == 0)
                break;
        }

        if(pid == 0)
        {
            if(pipenum)             //输入的命令含有管道
            {
                if(i == 0)          //第一个子进程
                {
                    dup2(pfd[0][1],STDOUT_FILENO); //重定向第一个进程的标准输出至管道写端
                    close(pfd[0][0]);               //关闭第一个进程的读端
                    for(int j = 1; j < pipenum; j++)    //关闭该进程的使用不到的其他管道
                    {
                        close(pfd[j][0]);
                        close(pfd[j][1]);
                    }
                }
                else if(i == pipenum)   //最后创建的子进程
                {
                   dup2(pfd[i-1][0],STDIN_FILENO); //重定向最后进程的标准输入至管道读端
                   close(pfd[i-1][1]);              //关闭写端
                   for(int j = 1; j < pipenum-1; j++)
                   {
                       close(pfd[j][0]);
                       close(pfd[j][1]);
                   }
                }
                else
                {
                    dup2(pfd[i-1][0], STDIN_FILENO);        //重定向中间进程的标准输入至管道读端
                    close(pfd[i-1][1]);                     //关闭管道写端

                    dup2(pfd[i][1], STDOUT_FILENO);         //重定向中间进程的标准输出至管道写端
                    close(pfd[i][0]);                       //关闭管道读端

                    for(j = 0; j < pipenum; j++)            //关闭不使用的管道读写两端
                    {
                        if(j != i || j != i - 1)
                        {
                            close(pfd[j][0]);
                            close(pfd[j][1]);
                        }
                    }
                }

            }
            if(cmd[i].in)                               //用户在命令中使用了输入重定向
            {
                fd = open(cmd[i].in, O_RDONLY);         //打开用户指定的重定向文件，只读即可
                if(fd != -1)
                {
                    dup2(fd, STDIN_FILENO);             //将标准输出重定向给该文件
                }
            }
            if(cmd[i].out)                              //用户在命令中使用了输出重定向
            {
                fd = open(cmd[i].out, O_WRONLY|O_CREAT|O_TRUNC, 0644);      //使用写权限打开用户制定的重定向文件
                if(fd != -1)
                {
                    dup2(fd, STDOUT_FILENO);        //将标准输出重定向给该文件
                }
            }

            execvp(cmd[i].argv[0], cmd[i].argv);     //执行用户输入的命令
            fprintf(stderr, "executing %s error.\n", cmd[i].argv[0]);
            exit(127);
        }
        
        //parent
        for(i = 0; i < pipenum; i++)               //父进程不参与命令执行，关闭其掌握的通道两端
        {
            close(pfd[i][0]);
            close(pfd[i][1]);
        }

        for(i = 0; i < cmdnum; i++)                 //循环回首子进程
        {
            wait(NULL);
        }

    }

    return 0;
}
