#ifndef UNMAKE_H
#define UNMAKE_H

#include <stdbool.h>

struct _um_Entry;

#define um_cmd(file, ...)                           \
    do {                                            \
        char *args[] = { file, __VA_ARGS__, NULL }; \
        _um_cmd(args, _UM_ARR_SZ(args));            \
    } while (0)

#define um_cmdf(dst, file, ...)                     \
    do {                                            \
        char *args[] = { file, __VA_ARGS__, NULL }; \
        _um_cmdf(dst, args, _UM_ARR_SZ(args));      \
    } while (0)

#define um_recipe(fn, ...) _um_recipe(fn, __VA_ARGS__, NULL)
#define um_init(argc, argv) _um_init(argc, argv, __FILE__)

bool um_build(const char *name);
void um_run(void);

#endif // UNMAKE_H

#define UNMAKE_IMPLEMENTATION
#ifdef UNMAKE_IMPLEMENTATION

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#define _UM_ARR_SZ(xs) (sizeof(xs) / sizeof(*xs))

#define _um_fork(blocking, block)               \
    do {                                        \
        pid_t _fid = fork();                    \
        switch (_fid) {                         \
        case -1: {                              \
            perror("ERROR: fork");              \
            exit(EXIT_FAILURE);                 \
        } break;                                \
                                                \
        case 0: {                               \
            do                                  \
                block while (0);                \
        } break;                                \
                                                \
        default: {                              \
            if (!blocking) break;               \
            if (waitpid(_fid, NULL, 0) == -1) { \
                perror("ERROR: waitpid");       \
                exit(EXIT_FAILURE);             \
            }                                   \
        } break;                                \
        }                                       \
    } while (0)

#define _UM_MAX_NB_DEPS 64
#define _UM_MAX_NB_ENTRIES 64

struct _um_Entry {
    const char *name;
    void (*fn)(void);
    size_t nb_deps;
    const char *deps[_UM_MAX_NB_DEPS];
};

struct {
    int argc;
    char **argv;
    const char *src;
    size_t nb_entries;
    struct _um_Entry entries[_UM_MAX_NB_ENTRIES];
} _um;

struct _um_Entry *_um_add_entry(const char *name, void (*fn)(void))
{
    assert(_um.nb_entries < _UM_MAX_NB_ENTRIES);
    struct _um_Entry *ret = &_um.entries[_um.nb_entries];
    _um.nb_entries += 1;

    ret->name = name;
    ret->fn = fn;

    return ret;
}

void _um_add_dep(struct _um_Entry *entry, const char *dep)
{
    assert(entry->nb_deps < _UM_MAX_NB_DEPS);
    entry->deps[entry->nb_deps] = dep;
    entry->nb_deps += 1;
}

struct _um_Entry *_um_find_entry(const char *name)
{
    for (size_t i = 0; i < _um.nb_entries; i += 1) {
        if (strcmp(_um.entries[i].name, name) == 0) {
            return &_um.entries[i];
        }
    }
    return NULL;
}

void _um_cmd(char **args, int argc)
{
    fputs("[CMD] ", stdout);
    for (int i = 0; i < argc - 2; i += 1) {
        printf("%s ", args[i]);
    }
    printf("%s\n", args[argc - 2]);

    _um_fork(true, {
        execvp(args[0], args);
        perror("ERROR: execlp");
        exit(EXIT_FAILURE);
    });
}

void _um_cmdf(char *dst, char **args, int argc)
{
    fputs("[CMD] ", stdout);
    for (int i = 0; i < argc - 2; i += 1) {
        printf("%s ", args[i]);
    }
    printf("%s > %s\n", args[argc - 2], dst);

    _um_fork(true, {
        int fd = creat(dst, 0664);
        if (fd == -1) {
            perror("ERROR: creat");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("ERROR: dup2");
            exit(EXIT_FAILURE);
        }

        execvp(args[0], args);
        perror("ERROR: execlp");
        exit(EXIT_FAILURE);
    });
}

void _um_recipe(void (*fn)(void), const char *entry_name, ...)
{
    struct _um_Entry *entry = _um_add_entry(entry_name, fn);

    va_list ap;
    va_start(ap, entry_name);
    for (const char *dep; (dep = va_arg(ap, const char *));) {
        _um_add_dep(entry, dep);
    }
    va_end(ap);
}

bool _um_build_rec(struct _um_Entry *entry)
{
    bool should_build = false;

    struct stat estat;
    if (stat(entry->name, &estat) == -1) {
        if (errno != ENOENT) {
            perror("ERROR: stat entry");
            exit(EXIT_FAILURE);
        }
        if (entry->fn == NULL) {
            fprintf(stderr, "ERROR: don't know how to build '%s'\n", entry->name);
            exit(EXIT_FAILURE);
        }
        should_build = true;
    }

    for (size_t i = 0; i < entry->nb_deps; i += 1) {
        struct stat dstat;
        if (stat(entry->deps[i], &dstat) == -1) {
            if (errno != ENOENT) {
                perror("ERROR: stat dep");
                exit(EXIT_FAILURE);
            }

            struct _um_Entry *dep = _um_find_entry(entry->deps[i]);
            if (!dep) {
                fprintf(stderr, "ERROR: don't know how to build '%s'\n", entry->deps[i]);
                exit(EXIT_FAILURE);
            }
            _um_build_rec(dep);
        }
        if (estat.st_mtime < dstat.st_mtime) {
            should_build = true;
        }
    }

    if (should_build) {
        entry->fn();
        return true;
    }
    return false;
}

bool um_build(const char *name)
{
    struct _um_Entry *entry = _um_find_entry(name);
    if (!entry) {
        fprintf(stderr, "ERROR: don't know how to build '%s'\n", name);
        exit(EXIT_FAILURE);
    }
    return _um_build_rec(entry);
}

void build_self(void)
{
    um_cmd("cc", "-Wpedantic", "-o", _um.argv[0], (char *)_um.src);
}

void _um_init(int argc, char **argv, const char *src)
{
    _um.argc = argc;
    _um.argv = argv;
    _um.src = src;

    um_recipe(build_self, argv[0], src);

    if (um_build(argv[0])) {
        argv[argc] = NULL;

        execvp(argv[0], argv);
        perror("ERROR: execvp");
        exit(EXIT_FAILURE);
    }
}

void um_run(void)
{
    if (_um.argc == 2) {
        um_build(_um.argv[1]);
    } else {
        fprintf(stderr, "Usage: %s recipe\n\n", _um.argv[0]);
        fputs("Available recipes:\n", stderr);
        for (size_t i = 0; i < _um.nb_entries; i += 1) {
            if (strcmp(_um.entries[i].name, _um.argv[0]) == 0) {
                continue;
            }
            fprintf(stderr, "    %s\n", _um.entries[i].name);
        }
        exit(EXIT_FAILURE);
    }
}

#endif // UNMAKE_IMPLEMENTATION
