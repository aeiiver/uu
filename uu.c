#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct exec {
    char *name;
    char *deps[32];
    char *cflags[32];
};

static void _uu_forkexec(struct exec exec)
{
    char *argv[64];
    {
        int i = 0;

        fputs("[CMD] ", stdout);

        argv[i] = "cc";
        printf("%s ", argv[i]);
        i += 1;

        for (int c = 0; exec.cflags[c]; c += 1) {
            argv[i] = exec.cflags[c];
            printf("%s ", argv[i]);
            i += 1;
        }

        argv[i] = "-o";
        printf("%s ", argv[i]);
        i += 1;

        argv[i] = exec.name;
        printf("%s ", argv[i]);
        i += 1;

        for (int d = 0; exec.deps[d]; d += 1) {
            argv[i] = exec.deps[d];
            printf("%s ", argv[i]);
            i += 1;
        }

        argv[i] = NULL;
        putchar('\n');
    }

    int pid = fork();
    switch (pid) {
    case 0:
        execvp(argv[0], argv);
        perror("ERROR: execvp");
        exit(EXIT_FAILURE);
    case -1:
        perror("ERROR: fork");
        exit(EXIT_FAILURE);
    default:
        if (waitpid(pid, NULL, 0) == -1) {
            perror("ERROR: waitpid");
            exit(EXIT_FAILURE);
        }
    }
}

static void uu(struct exec exec)
{
    struct stat st_exec;
    if (stat(exec.name, &st_exec) == -1) {
        if (errno != ENOENT) {
            perror("ERROR: stat exec");
            exit(EXIT_FAILURE);
        }
        _uu_forkexec(exec);
        return;
    }

    for (int i = 0; exec.deps[i]; i += 1) {
        struct stat st_dep;
        if (stat(exec.deps[i], &st_dep) == -1) {
            perror("ERROR: stat dep");
            exit(EXIT_FAILURE);
        }
        if (st_exec.st_mtime < st_dep.st_mtime) {
            _uu_forkexec(exec);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;

    uu((struct exec) {
        argv[0],
        { __FILE__ },
        { "-O3", "-std=c17" },
    });

    return EXIT_SUCCESS;
}
