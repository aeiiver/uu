#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// FIXME: These headers might cause some issues but my current setup didn't
//        catch any. Check if this works on a real windows machine.
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
#  include <sys/wait.h>
#elif defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  error "Unsupported system"
#endif

#define err(msg) (perror("ERROR: " msg), exit(EXIT_FAILURE))

typedef struct {
    char *name;
    char *files[32];
    char *opts[32];
} exec;

#if defined(__linux__)
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
#elif defined(_WIN32)
static int uu_forkexec(char *argv)
{
    STARTUPINFO si = { .cb = sizeof(si), .dwFlags = STARTF_USESTDHANDLES };
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcess(NULL, argv, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == 0) {
        fprintf(stderr, "ERROR: CreateProcess: Failed with error 0x%lx\n", GetLastError());
        exit(EXIT_FAILURE);
    }
    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED) {
        fprintf(stderr, "ERROR: WaitForSingleObject: Failed with error 0x%lx\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    DWORD status;
    if (GetExitCodeProcess(pi.hProcess, &status) == 0) {
        fprintf(stderr, "ERROR: GetExitCodeProcess: Failed with error 0x%lx\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    // FIXME: I don't know, should we check for errors?
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return status;
}
#endif

static int uu_cc(exec exec)
{
#if defined(__linux__)
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
#elif defined(_WIN32)
    // FIXME: Change this hard-coded dirty test
    char argv[256] = "cl.exe -o other " __FILE__;
    printf("[CMD] %s\n", argv);
#endif
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
