#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *name;

void find(char *path)
{
    char *p = path + strlen(path);
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open \n");
        return;
    }
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {

        if (de.inum == 0)
            continue;
        //   memmove(p, de.name, DIRSIZ);
        strcpy(p, de.name);
        // p[DIRSIZ] = 0;
        if (stat(path, &st) < 0) // 因为没有用chdir（没有getpwd,不能chdir），只能是绝对路径
        {
            printf("find: cannot stat %s\n", p);
            continue;
        }
        if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0)
        {
            find(path);
        }
        else if (strcmp(p, name) == 0)
        {
            printf("%s\n", path);
        }
    }
    close(fd);
    p = p - 1; // 没有--p
    memset(p, '\0', strlen(p));
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        fprintf(2, "find: no use arguments ...\n");
        exit(1);
    }
    char path[512] = {};
    if (argc == 2)
    {
        path[0] = '.';
        name = argv[1];
    }
    if (argc == 3)
    {
        strcpy(path, argv[1]);
        name = argv[2];
    }
    find(path);
    exit(0);
}
