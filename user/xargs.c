#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        fprintf(2, "Usage: less arguments ...\n");
        exit(1);
    }
    char **cmd = 0;
    int cmd_num = 0;
    for (; cmd_num < argc - 1; cmd_num++)
    {
        cmd[cmd_num] = argv[cmd_num + 1];
        // strcpy(cmd[cmd_num], argv[cmd_num]);
    }
    char buf[512];

    // read(0, buf, 512);不知道为什么一次读不完
    int n = 0;
    while (read(0, buf + n, 1))
    {
        if (n == 1023)
        {
            fprintf(2, "argument is too long\n");
            exit(1);
        }
        // if (buf[n] == '\n')
        // {
        // 	break;
        // }这样就不用后面再判断了，这块判断完直接fork 执行
        n++;
    }
    char *p = buf;
    int len = strlen(buf);

    int cun_len = 0;
    for (int j = 0; j <= len; j++)
    {
        if ((p[j] == '\n' || p[j] == '\0') && cun_len != 0)
        {
            p[j] = '\0';
            cmd[cmd_num] = p + j - cun_len;
            cmd[cmd_num + 1] = 0;
            switch (fork())
            {
            case -1:
            {
                fprintf(2, "fork() error!\n");
                exit(1);
                break;
            }
            case 0:
            {
                exec(cmd[0], cmd);
                fprintf(2, "exec failed\n");
                exit(1);
                break;
            }
            default:
            {
                wait(0);
                break;
            }
            }
            cun_len = 0;
            continue;
        }
        cun_len++;
    }

    exit(0);
}

// 作为参考：https://github.com/PKUFlyingPig/MIT6.S081-2020fall/blob/master/reports/Utils.md

// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// #include "kernel/param.h"

// int readline(char *new_argv[32], int curr_argc){
// 	char buf[1024];
// 	int n = 0;
// 	while(read(0, buf+n, 1)){
// 		if (n == 1023)
// 		{
// 			fprintf(2, "argument is too long\n");
// 			exit(1);
// 		}
// 		if (buf[n] == '\n')
// 		{
// 			break;
// 		}
// 		n++;
// 	}
// 	buf[n] = 0;
// 	if (n == 0)return 0;
// 	int offset = 0;
// 	while(offset < n){
// 		new_argv[curr_argc++] = buf + offset;
// 		while(buf[offset] != ' ' && offset < n){把空格分割开的 作为新的参数
// 			offset++;
// 		}
// 		while(buf[offset] == ' ' && offset < n){
// 			buf[offset++] = 0;
// 		}
// 	}
// 	return curr_argc;
// }

// int main(int argc, char const *argv[])
// {
// 	if (argc <= 1)
// 	{
// 		fprintf(2, "Usage: xargs command (arg ...)\n");
// 		exit(1);
// 	}
// 	char *command = malloc(strlen(argv[1]) + 1);
// 	char *new_argv[MAXARG];
// 	strcpy(command, argv[1]);
// 	for (int i = 1; i < argc; ++i)
// 	{
// 		new_argv[i - 1] = malloc(strlen(argv[i]) + 1);
// 		strcpy(new_argv[i - 1], argv[i]);
// 	}

// 	int curr_argc;
// 	while((curr_argc = readline(new_argv, argc - 1)) != 0)
// 	{
// 		new_argv[curr_argc] = 0;
// 		if(fork() == 0){
// 			exec(command, new_argv);
// 			fprintf(2, "exec failed\n");
// 			exit(1);
// 		}
// 		wait(0);
// 	}
// 	exit(0);
// }
