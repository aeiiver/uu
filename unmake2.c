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
#define _UM_NB_CFLAGS_MAX 32
#define _UM_STRLEN_CFLAGS_MAX 256

struct _um_cflags {
    size_t nb_args;
    char *args[_UM_NB_CFLAGS_MAX];
    size_t strlen;
    char str[_UM_STRLEN_CFLAGS_MAX];
};

struct _um_exec {
    const char *name;
    size_t nb_deps;
    const char *deps[_UM_NB_DEPS_MAX];
    struct _um_cflags cflags;
};

static struct {
    size_t nb_execs;
    struct _um_exec execs[_UM_NB_EXECS_MAX];
    struct _um_cflags cflags;
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

static struct _um_exec *_um_find_executable(const char *name)
{
    for (size_t i = 0; i < _um.nb_execs; i += 1) {
        if (strcmp(_um.execs[i].name, name) == 0) {
            return &_um.execs[i];
        }
    }
    return NULL;
}

static void _um_cflags(const char *file, const char *cflags, size_t cflags_len)
{
    struct _um_cflags *cf;
    if (file) {
        struct _um_exec *exec = _um_find_executable(file);
        if (exec) {
            cf = &exec->cflags;
        } else {
            fprintf(stderr,
                    "[WARN] Attempted to set CFLAGS to non-registered executable '%s'.\n"
                    "[WARN] Falling back to global CFLAGS. Make sure you do call um_cflags *after* um_executable.\n",
                    file);
            cf = &_um.cflags;
        }
    } else {
        cf = &_um.cflags;
    }

    assert(cflags_len <= _UM_STRLEN_CFLAGS_MAX);
    cf->strlen = cflags_len;
    memcpy(cf->str, cflags, cf->strlen);

    for (size_t i = 0; i < cf->strlen; i += 1) {
        if (cf->str[i] == ' ') {
            cf->str[i] = 0;
            continue;
        }

        assert(cf->nb_args < _UM_NB_CFLAGS_MAX);
        cf->args[cf->nb_args] = &cf->str[i];
        cf->nb_args += 1;

        for (; cf->str[i] && cf->str[i] != ' '; i += 1) { }
        if (cf->str[i]) {
            i -= 1;
        }
    }
}

#define um_cflags(file, cflags) _um_cflags(file, cflags, sizeof(cflags))

static void _um_build(struct _um_exec *exec)
{
    struct _um_cflags cf;
    if (exec->cflags.nb_args > 0) {
        cf = exec->cflags;
    } else {
        cf = _um.cflags;
    }

    size_t nb_args = 3 + exec->nb_deps + cf.nb_args + 1;
    char *args[nb_args];
    args[0] = "cc";
    args[1] = "-o";
    args[2] = (char *)exec->name;
    memcpy(&args[3], exec->deps, sizeof(*exec->deps) * exec->nb_deps);
    memcpy(&args[3 + exec->nb_deps], cf.args, sizeof(*cf.args) * cf.nb_args);
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

int main(int argc, char **argv)
{
    (void)argc;

    um_executable(argv[0], __FILE__);
    um_cflags(argv[0], " -O0 -ggdb -std=c17"
                       " -Wall -Wextra"
                       " -Wpedantic -pedantic-errors"
                       " -Wmissing-prototypes -Wshadow=local"
                       " -Wconversion -Warith-conversion"
                       " -Werror");

    um_run();

    return EXIT_SUCCESS;
}
