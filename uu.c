#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define err(msg) (perror("ERROR: " msg), exit(EXIT_FAILURE))

typedef struct {
    char *name;
    char *files[32];
    char *opts[32];
} exec;

static int uu_forkexec(char **argv)
{
    int pid = fork();
    switch (pid) {
    case 0:
        execvp(argv[0], argv);
        err("execvp");
    case -1:
        err("fork");
    default: {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            err("waitpid");
        }
        if (!WIFEXITED(status)) {
            fputs("ERROR: waitpid: Child caught deadly signal\n", stderr);
            exit(EXIT_FAILURE);
        }
        return WEXITSTATUS(status);
    }
    }
}

static int uu_cc(exec exec)
{
    char *argv[64];
    {
        int i = 0;

        fputs("[CMD] ", stdout);

        argv[i] = "cc";
        printf("%s ", argv[i]);
        i += 1;

        for (int o = 0; exec.opts[o]; o += 1) {
            argv[i] = exec.opts[o];
            printf("%s ", argv[i]);
            i += 1;
        }

        argv[i] = "-o";
        printf("%s ", argv[i]);
        i += 1;

        argv[i] = exec.name;
        printf("%s ", argv[i]);
        i += 1;

        for (int f = 0; exec.files[f]; f += 1) {
            argv[i] = exec.files[f];
            printf("%s ", argv[i]);
            i += 1;
        }

        argv[i] = NULL;
        putchar('\n');
    }
    return uu_forkexec(argv);
}

static int uu(exec exec)
{
    struct stat st_exec;
    if (stat(exec.name, &st_exec) < 0) {
        if (errno != ENOENT) {
            err("stat exec");
        }
        return uu_cc(exec) == 0;
    }

    for (int i = 0; exec.files[i]; i += 1) {
        struct stat st_file;
        if (stat(exec.files[i], &st_file) < 0) {
            err("stat file");
        }
        if (st_exec.st_mtime < st_file.st_mtime) {
            return uu_cc(exec) == 0;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (uu((exec) { argv[0], { __FILE__ }, { "-O3", "-std=c17" } })) {
        execvp(argv[0], (char *[]) { argv[0], NULL });
        err("execvp on self");
    }

    return EXIT_SUCCESS;
}
