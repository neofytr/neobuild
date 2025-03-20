#ifndef NEOBUILD_H
#define NEOBUILD_H

#include "dynarr/inc/dynarr.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

// for pid_t and mode_t
#include <sys/types.h>

#include <stdio.h>

typedef enum
{
    ERROR,
    WARNING,
    INFO,
    DEBUG
} neolog_level_t;

typedef struct
{
    char *config;
    char *value;
} neoconfig_t;

#define NEO_LOG(level, msg)                         \
    do                                              \
    {                                               \
        switch (level)                              \
        {                                           \
        case ERROR:                                 \
            fprintf(stderr, "[ERROR] %s\n", msg);   \
            break;                                  \
        case WARNING:                               \
            fprintf(stderr, "[WARNING] %s\n", msg); \
            break;                                  \
        case INFO:                                  \
            fprintf(stdout, "[INFO] %s\n", msg);    \
            break;                                  \
        case DEBUG:                                 \
            fprintf(stdout, "[DEBUG] %s\n", msg);   \
            break;                                  \
        default:                                    \
            fprintf(stdout, "[UNKNOWN] %s\n", msg); \
            break;                                  \
        }                                           \
    } while (0)

// check if the neo.c build C file has changed since the previous compilation of it to neo
// (done by checking the modified date/time of neo.c; if this time comes after the last modified of neo.c, we need to rebuild neo from this new neo.c)

// buildneo, build.c and build should be in the same directory
bool neorebuild(const char *build_file, char **argv);

/**
 * Enum representing different shell types.
 */
typedef enum
{
    DASH, /**< Dash shell */
    BASH, /**< Bash shell */
    SH    /**< Standard shell (sh) */
} neoshell_t;

/**
 * Structure representing a command to be executed.
 */
typedef struct
{
    dyn_arr_t *args;  /**< Dynamic array storing command arguments. */
    neoshell_t shell; /**< Shell type used to execute the command. */
} neocmd_t;

/**
 * Appends arguments to a command structure.
 *
 * This macro simplifies appending multiple arguments to a `neocmd_t` object.
 * It automatically adds a terminating `NULL` argument.
 *
 * @param neocmd_ptr Pointer to the `neocmd_t` object.
 * @param ... Variable arguments representing the command arguments to append.
 */
#define neocmd_append(neocmd_ptr, ...) neocmd_append_null((neocmd_ptr), __VA_ARGS__, NULL)

/**
 * Generates a string representation of a label, ensuring compatibility with filenames containing whitespaces.
 *
 * @param label The label to be converted to a string.
 * @return String literal representation of the label.
 */
#define LABEL_WITH_SPACES(label) #label

/**
 * Creates a new command structure.
 *
 * @param shell The shell type to be used for executing the command.
 * @return Pointer to a newly allocated `neocmd_t` structure.
 */
neocmd_t *neocmd_create(neoshell_t shell);

/**
 * Deletes a command structure and frees allocated resources.
 *
 * @param neocmd Pointer to the `neocmd_t` object to be deleted.
 * @return true if the command was successfully deleted, false otherwise.
 */
bool neocmd_delete(neocmd_t *neocmd);

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

/**
 * Runs a command asynchronously.
 *
 * This function forks a new process to execute the command in the background.
 *
 * @param neocmd Pointer to the command structure to be executed.
 * @return The process ID (`pid_t`) of the child process if successful, or `-1` on failure.
 */
pid_t neocmd_run_async(neocmd_t *neocmd);

/**
 * Runs the given command synchronously and waits for it to complete.
 *
 * @param neocmd Pointer to the command structure.
 * @param status Pointer to an integer where the process exit status will be stored.
 *               - If the process exits normally, `*status` will hold the exit code (0-255).
 *               - If the process is terminated by a signal, `*status` will hold the signal number.
 * @param code Pointer to an integer where the termination reason will be stored.
 *             - It will be set to one of the `si_code` values (e.g., `CLD_EXITED`, `CLD_KILLED`, `CLD_DUMPED`, etc.).
 * @param print_status_desc If `true`, prints a description of the termination status.
 *
 * @return `true` if the process was successfully waited on, `false` otherwise.
 */
bool neocmd_run_sync(neocmd_t *neocmd, int *status, int *code, bool print_status_desc);

/**
 * Waits for a child process to terminate.
 *
 * @param pid The process ID (`pid_t`) of the child process to wait for.
 * @param status Pointer to an integer where the process exit status will be stored.
 *               - If the process exits normally, `*status` will hold the exit code (0-255).
 *               - If the process is terminated by a signal, `*status` will hold the signal number.
 * @param code Pointer to an integer where the termination reason will be stored.
 *             - It will be set to one of the `si_code` values (e.g., `CLD_EXITED`, `CLD_KILLED`, `CLD_DUMPED`, etc.).
 * @param should_print If `true`, prints information about the process termination.
 *
 * @return `true` if the process was successfully waited on, `false` otherwise.
 */
bool neoshell_wait(pid_t pid, int *status, int *code, bool should_print);

/**
 * Appends arguments to a command structure.
 *
 * This function allows appending multiple arguments to a command dynamically.
 * The arguments list must be NULL-terminated.
 *
 * @param neocmd Pointer to the `neocmd_t` object.
 * @param ... Variable argument list representing the command arguments.
 * @return `true` if the arguments were successfully appended, `false` otherwise.
 */
bool neocmd_append_null(neocmd_t *neocmd, ...);

/**
 * Generates a string representation of the command.
 *
 * This function converts a `neocmd_t` object into a formatted string representation.
 * The caller is responsible for freeing the returned string using `free()`.
 *
 * @param neocmd Pointer to the command structure.
 * @return A dynamically allocated string containing the command representation.
 */
const char *neocmd_render(neocmd_t *neocmd);

bool neo_mkdir(const char *dir_path, mode_t mode);

neoconfig_t *neo_parse_config(const char *config_file_path, size_t *config_arr_len);

bool neo_free_config(neoconfig_t *config_arr, size_t config_arr_len);

neoconfig_t *neo_parse_config_arg(char **argv, size_t *config_arr_len);

#ifdef NEO_REMOVE_PREFIX

#define cmd_create neocmd_create
#define cmd_delete neocmd_delete
#define cmd_run_async neocmd_run_async
#define cmd_run_sync neocmd_run_sync
#define cmd_append neocmd_append
#define cmd_append_null neocmd_append_null
#define cmd_render neocmd_render
#define shell_wait neoshell_wait

#endif /* NEO_REMOVE_PREFIX */

#endif /* NEOBUILD_H */
