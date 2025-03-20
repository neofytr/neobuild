#include "neobuild.h"
#include "strix/header/strix.h"

// for pipe
#include <unistd.h>

// for wait
#include <sys/wait.h>

// for bool
#include <stdbool.h>

// for errno
#include <errno.h>

static inline void cleanup_arg_array(dyn_arr_t *arr)
{
    for (int64_t index = 0; index <= (int64_t)(arr)->last_index; index++)
    {
        strix_t *temp;
        if (dyn_arr_get((arr), index, &temp)) // we will inevitably be leaking memory if dyn_arr_get fails for some index and that index is allocated
        {
            strix_free(temp);
        }
    }
}

#define APPEND_CLEANUP(arr)     \
    do                          \
    {                           \
        strix_free(arg_strix);  \
        cleanup_arg_array(arr); \
        va_end(args);           \
        return false;           \
    } while (0)

const char *cmd_render(cmd_t *cmd)
{
    if (!cmd || !cmd->args)
    {
        return NULL;
    }

    strix_t *strix = strix_create_empty();
    if (!strix)
    {
        return NULL;
    }

    dyn_arr_t *arr = cmd->args;
    int64_t last = arr->last_index;

    for (int64_t index = 0; index <= last; index++)
    {
        strix_t *temp;
        if (!dyn_arr_get(arr, index, &temp))
        {
            strix_free(strix);
            return NULL;
        }

        if (!strix_concat(strix, temp))
        {
            strix_free(strix);
            return NULL;
        }

        if (!strix_append(strix, " "))
        {
            strix_free(strix);
            return NULL;
        }
    }

    char *str = strix_to_cstr(strix);
    if (!str)
    {
        strix_free(strix);
        return NULL;
    }

    strix_free(strix);
    return (const char *)str;
}

bool shell_wait(pid_t pid, int *status, int *code, bool should_print)
{
    // check for invalid arguments
    if (pid < 0)
    {
        return false;
    }

    siginfo_t info;
    // wait for the child process with the given pid to exit or stop
    if (waitid(P_PID, (id_t)pid, &info, WEXITED | WSTOPPED) == -1)
    {
        if (should_print)
            fprintf(stderr, "waitid on pid %d failed: %s\n", pid, strerror(errno));
        return false;
    }

    // store the termination reason and status
    if (code)
    {
        *code = info.si_code;
    }

    if (status)
    {
        *status = info.si_status;
    }

    // check how the child process terminated
    switch (info.si_code)
    {
    case CLD_EXITED:
        // child exited normally, store the exit status
        if (should_print)
            fprintf(stderr, "[CMD] shell process %d exited normally with status %d\n", pid, info.si_status);
        break;

    case CLD_KILLED:
        // child was killed by a signal
        if (should_print)
            fprintf(stderr, "[ERROR] shell process %d was killed by signal %d\n", pid, info.si_status);
        break;

    case CLD_DUMPED:
        // child was killed by a signal and dumped core
        if (should_print)
            fprintf(stderr, "[ERROR] shell process %d was killed by signal %d (core dumped)\n", pid, info.si_status);
        break;

    case CLD_STOPPED:
        // child was stopped by a signal
        if (should_print)
            fprintf(stderr, "[ERROR] shell process %d was stopped by signal %d\n", pid, info.si_status);
        break;

    case CLD_TRAPPED:
        // traced child has trapped (e.g., during debugging)
        if (should_print)
            fprintf(stderr, "[ERROR] shell process %d was trapped by signal %d (traced child)\n", pid, info.si_status);
        break;

    default:
        // unknown or unexpected termination reason
        if (should_print)
            fprintf(stderr, "[ERROR] shell process %d terminated in an unknown way (si_code: %d, si_status: %d)\n",
                    pid, info.si_code, info.si_status);
        return false;
    }

    return true;
}

#define READ_END 0
#define WRITE_END 1

#define CLOSE_PIPE(pipe)          \
    do                            \
    {                             \
        close((pipe)[READ_END]);  \
        close((pipe)[WRITE_END]); \
    } while (false)

/*
 * This function runs a command asynchronously by forking a child process.
 *
 * - The child process will execute independently and will not be waited for within this function.
 * - The parent must explicitly call waitpid(pid) later to retrieve the exit status.
 * - If the parent process exits before the child, the child process will be reparented to init (PID 1),
 *   which will eventually clean it up.
 * - If the parent does not call waitpid(), the child remains in a "zombie" state after termination.
 *   - A zombie process retains only its PID and exit status in the process table.
 *   - It no longer executes or consumes memory, but it persists until the parent calls waitpid().
 *   - If the parent itself terminates, init adopts the zombie process and clears it.
 * - All process resources (memory, file descriptors, etc.) are freed upon child exit,
 *   except for the exit status, which remains in the process table until reaped.
 */
pid_t cmd_run_async(cmd_t *cmd)
{
    // returns -1 if an error occurred

    if (!cmd)
    {
        return -1;
    }

    const char *command = cmd_render(cmd);
    if (!command)
    {
        return -1;
    }

    fprintf(stdout, "[CMD] %s\n", command); // display the command being run by the newly created shell

    pid_t child = fork();

    if (child == -1)
    {
        // no child process is created
        fprintf(stderr, "[ERROR] Child process could not be forked: %s\n", strerror(errno));
        free((void *)command);
        return -1;
    }
    else if (!child)
    {
        // child process
        switch (cmd->shell)
        {
        case BASH:
        {
            char *argv[4] = {"/bin/bash", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the cmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/bash", argv) == -1)
            {
                fprintf(stderr, "[ERROR] Child shell could not be executed: %s\n", strerror(errno));
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        case SH:
        {
            char *argv[4] = {"/bin/sh", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the cmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/sh", argv) == -1)
            {
                fprintf(stderr, "[ERROR] Child shell could not be executed: %s\n", strerror(errno));
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        case DASH:
        {
            char *argv[4] = {"/bin/dash", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the cmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/dash", argv) == -1)
            {
                fprintf(stderr, "[ERROR] Child shell could not be executed: %s\n", strerror(errno));
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        default:
        {
            // execute BASH in the default case
            char *argv[4] = {"/bin/bash", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the cmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/bash", argv) == -1)
            {
                fprintf(stderr, "[ERROR] Child shell could not be executed: %s\n", strerror(errno));
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        }
    }
    else
    {
        // parent process; immediately return the pid_t
        free((void *)command);
        return child;
    }

    // would not be reached
    free((void *)command);
    return -1;
}

bool cmd_run_sync(cmd_t *cmd, int *status, int *code, bool print_status_desc)
{
    // the parent and child share the same open file descriptions and file descriptors
    // for stdin, stdout, and stderr

    pid_t child = cmd_run_async(cmd);
    if (child == -1)
    {
        return false;
    }

    shell_wait(child, status, code, print_status_desc);
    return true;
}

#undef READ_END
#undef WRITE_END

cmd_t *cmd_create(shell_t shell)
{
    cmd_t *cmd = (cmd_t *)malloc(sizeof(cmd_t));
    if (!cmd)
    {
        return NULL;
    }

#define MIN_ARG_NUM 16
    cmd->args = dyn_arr_create(MIN_ARG_NUM, sizeof(strix_t *), NULL);
#undef MIN_ARG_NUM
    if (!cmd->args)
    {
        free(cmd);
        return NULL;
    }
    cmd->shell = shell;

    return cmd;
}

bool cmd_delete(cmd_t *cmd)
{
    if (!cmd || !cmd->args)
    {
        return false;
    }

    cleanup_arg_array(cmd->args);

    dyn_arr_free(cmd->args);
    free((void *)cmd);

    return true;
}

bool cmd_append_null(cmd_t *cmd, ...)
{
    if (!cmd || !cmd->args)
    {
        return false;
    }

    va_list args;
    va_start(args, cmd); // the variadic arguments start after the parameter cmd; initialize the list with the last static arguments
    dyn_arr_t *cmd_args = cmd->args;

    const char *arg = va_arg(args, const char *);
    while (arg)
    {
        strix_t *arg_strix = strix_create(arg);
        if (!arg_strix)
        {
            cleanup_arg_array(cmd_args);
            va_end(args);
            return false;
        }

        if (!dyn_arr_append(cmd_args, &arg_strix))
        {
            APPEND_CLEANUP(cmd_args);
        }
        arg = va_arg(args, const char *);
    }

    va_end(args); // finished extracting all variadic arguments; cleanup
    return true;
}