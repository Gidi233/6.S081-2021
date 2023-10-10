#include "kernel/types.h"
#include "user/user.h"

void newfork(int lfd)
{
    int num;
    if (read(lfd, &num, 4) == 4)
        printf("prime %d\n", num);
    else
        return;

    int r[2];

    if (pipe(r) < 0)
    {
        exit(1);
    }
    switch (fork())
    {
    case -1:
    {
        fprintf(2, "fork() error!\n");
        close(r[0]);
        close(r[1]);
        exit(1);
        break;
    }
    case 0:
    {
        close(r[1]);
        newfork(r[0]);
        close(r[0]);
        exit(0);
        break;
    }
    default:
    {
        close(r[0]);
        int i;
        while (read(lfd, &i, 4) == 4)
        {
            if (i % num)
            {
                write(r[1], &i, 4);
            }
        }
        close(r[1]);
        // if (num != 31)
        wait(0);
        break;
    }
    }
}
int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "Usage: no use arguments ...\n");
        exit(1);
    }
    int r[2];
    if (pipe(r) < 0)
    {
        exit(1);
    }
    switch (fork())
    {
    case -1:
    {
        fprintf(2, "fork() error!\n");
        close(r[0]);
        close(r[1]);
        exit(1);
        break;
    }
    case 0:
    {
        close(r[1]);
        newfork(r[0]);
        close(r[0]);
        break;
    }
    default:
    {
        close(r[0]);
        for (int i = 2; i <= 35; i++)
        {
            if (i % 2)
            {
                write(r[1], &i, 4);
            }
        }
        close(r[1]);
        wait(0);
        break;
    }
    }
    exit(0);
}
