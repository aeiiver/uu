#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define _UM_NB_EXECS_MAX 32
#define _UM_NB_DEPS_MAX 128

struct _um_exec {
    const char *name;
    size_t nb_deps;
    const char *deps[_UM_NB_DEPS_MAX];
};

static struct {
    size_t nb_execs;
    struct _um_exec execs[_UM_NB_EXECS_MAX];
} _um;

static void _um_executable(const char *name, const char **files)
{
    assert(_um.nb_execs < _UM_NB_EXECS_MAX);
    struct _um_exec *exec = &_um.execs[_um.nb_execs];
    _um.nb_execs += 1;

    exec->name = name;
    for (size_t i = 0; files[i]; i += 1) {
        assert(exec->nb_deps < _UM_NB_DEPS_MAX);
        exec->deps[i] = files[i];
        exec->nb_deps += 1;
    }
}

#define um_executable(name, ...) _um_executable(name, (const char *[]) { __VA_ARGS__, NULL })

static void _um_build(struct _um_exec *exec)
{
    size_t nb_args = exec->nb_deps + 4;
    char *args[nb_args];
    args[0] = "cc";
    args[1] = "-o";
    args[2] = (char *)exec->name;
    memcpy(&args[3], exec->deps, sizeof(*exec->deps) * exec->nb_deps);
    args[nb_args - 1] = NULL;

    fputs("[CMD] ", stdout);
    for (size_t i = 0; i < nb_args - 2; i += 1) {
        printf("%s ", args[i]);
    }
    printf("%s\n", args[nb_args - 2]);

    pid_t fid = fork();
    switch (fid) {
    case 0:
        execvp(args[0], args);
        perror("ERROR: execvp");
        exit(EXIT_FAILURE);
    case -1:
        perror("ERROR: fork");
        exit(EXIT_FAILURE);
    default:
        if (waitpid(fid, NULL, 0) == -1) {
            perror("ERROR: waitpid");
            exit(EXIT_FAILURE);
        }
    }
}

static void _um_try_build(struct _um_exec *exec)
{
    struct stat st_exec;
    if (stat(exec->name, &st_exec) == -1) {
        if (errno != ENOENT) {
            perror("ERROR: Failed to build exec");
            exit(EXIT_FAILURE);
        }
        _um_build(exec);
        return;
    }

    for (size_t i = 0; i < exec->nb_deps; i += 1) {
        struct stat st_dep;
        if (stat(exec->deps[i], &st_dep) == -1) {
            perror("ERROR: Failed to find dep for exec");
            exit(EXIT_FAILURE);
        }
        if (st_exec.st_mtime < st_dep.st_mtime) {
            _um_build(exec);
            return;
        }
    }
}

static void um_run(void)
{
    for (size_t i = 0; i < _um.nb_execs; i += 1) {
        _um_try_build(&_um.execs[i]);
    }
}

/*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/ /*/*/

int main(void)
{
    um_executable("um", "ooo");
    um_run();

    return EXIT_SUCCESS;
}
