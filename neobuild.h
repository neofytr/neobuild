#ifndef NEOBUILD_H
#define NEOBUILD_H

#include "dynarr/inc/dynarr.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

/**
 * Enum representing different shell types.
 */
typedef enum
{
    DASH, /**< Dash shell */
    BASH, /**< Bash shell */
    SH    /**< Standard shell (sh) */
} shell_t;

/**
 * Structure representing a command to be executed.
 */
typedef struct
{
    dyn_arr_t *args; /**< Dynamic array storing command arguments. */
    shell_t shell;   /**< Shell type used to execute the command. */
} cmd_t;

/**
 * Appends arguments to a command structure.
 *
 * This macro simplifies appending multiple arguments to a `cmd_t` object.
 * It automatically adds a terminating `NULL` argument.
 *
 * @param cmd_ptr Pointer to the `cmd_t` object.
 * @param ... Variable arguments representing the command arguments to append.
 */
#define cmd_append(cmd_ptr, ...) cmd_append_null((cmd_ptr), __VA_ARGS__, NULL)

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
 * @return Pointer to a newly allocated `cmd_t` structure.
 */
cmd_t *cmd_create(shell_t shell);

/**
 * Deletes a command structure and frees allocated resources.
 *
 * @param cmd Pointer to the `cmd_t` object to be deleted.
 * @return true if the command was successfully deleted, false otherwise.
 */
bool cmd_delete(cmd_t *cmd);

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
 * @param cmd Pointer to the command structure to be executed.
 * @return The process ID (`pid_t`) of the child process if successful, or `-1` on failure.
 */
pid_t cmd_run_async(cmd_t *cmd);

/**
 * Runs the given command synchronously and waits for it to complete.
 *
 * @param cmd Pointer to the command structure.
 * @param status Pointer to an integer where the process exit status will be stored.
 *               - If the process exits normally, `*status` will hold the exit code (0-255).
 *               - If the process is terminated by a signal, `*status` will hold the signal number.
 * @param code Pointer to an integer where the termination reason will be stored.
 *             - It will be set to one of the `si_code` values (e.g., `CLD_EXITED`, `CLD_KILLED`, `CLD_DUMPED`, etc.).
 * @param print_status_desc If `true`, prints a description of the termination status.
 *
 * @return `true` if the process was successfully waited on, `false` otherwise.
 */
bool cmd_run_sync(cmd_t *cmd, int *status, int *code, bool print_status_desc);

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
bool shell_wait(pid_t pid, int *status, int *code, bool should_print);

/**
 * Appends arguments to a command structure.
 *
 * This function allows appending multiple arguments to a command dynamically.
 * The arguments list must be NULL-terminated.
 *
 * @param cmd Pointer to the `cmd_t` object.
 * @param ... Variable argument list representing the command arguments.
 * @return `true` if the arguments were successfully appended, `false` otherwise.
 */
bool cmd_append_null(cmd_t *cmd, ...);

/**
 * Generates a string representation of the command.
 *
 * This function converts a `cmd_t` object into a formatted string representation.
 * The caller is responsible for freeing the returned string using `free()`.
 *
 * @param cmd Pointer to the command structure.
 * @return A dynamically allocated string containing the command representation.
 */
const char *cmd_render(cmd_t *cmd);

#endif /* NEOBUILD_H */
