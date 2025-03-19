#include "neobuild.h"
#include "strix/header/strix.h"

// for pipe
#include <unistd.h>

// for pid_t
#include <sys/types.h>

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
        return child;
    }

    // would not be reached
    return -1;
}

bool cmd_run_sync(cmd_t *cmd, int *exit_code)
{
    // the parent and child share the same open file descriptions and file descriptors
    // for stdin, stdout, and stderr

    if (!cmd || !exit_code)
    {
        return false;
    }

    const char *command = cmd_render(cmd);
    if (!command)
    {
        return false;
    }

    fprintf(stdout, "[CMD] %s\n", command); // display the command being run by the newly created shell

    pid_t child = fork();

    if (child == -1)
    {
        // no child process is created
        fprintf(stderr, "[ERROR] Child process could not be forked: %s\n", strerror(errno));
        free((void *)command);
        return false;
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
        // parent process

        // the shell will exit and the return code of the shell will be stored in exit_code
        // we will wait for the shell to return since this is cmd run synchronous
        wait(exit_code);
        return true;
    }

    // would not be reached
    return false;
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