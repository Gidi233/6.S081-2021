#include "kernel/types.h"
#include "user/user.h"

void recieve(int fd)
{
    char buf[1];
    int pid = getpid();
    if (read(fd, buf, 1) == 1)
    {
        printf("%d: received p%cng\n", pid, buf[0]);
    }
    else
    {
        printf("%d: go wrong", pid);
        exit(1);
    }
}
int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "Usage: no use arguments ...\n");
        exit(1);
    }
    int p[2], s[2];
    if (pipe(p) < 0)
    {
        exit(1);
    }
    if (pipe(s) < 0)
    {
        exit(1);
    }
    switch (fork())
    {
    case -1:
    {
        fprintf(2, "fork() error!\n");
        close(p[0]);
        close(p[1]);
        close(s[0]);
        close(s[1]);
        exit(1);
        break;
    }
    case 0:
    {
        close(p[1]);
        close(s[0]);
        recieve(p[0]);
        close(p[0]);
        write(s[1], "o", 1);
        close(s[1]);
        break;
    }
    default:
    {
        close(p[0]);
        close(s[1]);
        write(p[1], "i", 1);
        close(p[1]);
        recieve(s[0]);
        close(s[0]);
        break;
    }
    }
    exit(0);
}
