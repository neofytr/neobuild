#ifndef NEOBUILD_H
#define NEOBUILD_H

#include "dynarr/inc/dynarr.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

typedef enum
{
    DASH,
    BASH,
    SH,
} shell_t;

typedef struct
{
    dyn_arr_t *args;
    shell_t shell;
} cmd_t;

#define cmd_append(cmd_ptr, ...) cmd_append_null((cmd_ptr), __VA_ARGS__, NULL);

cmd_t *cmd_create(shell_t shell);
bool cmd_delete(cmd_t *cmd);
pid_t cmd_run_async(cmd_t *cmd);
bool cmd_run_sync(cmd_t *cmd, int *exit_code); // it's the responsibility of the user to free the char * ptr which contains the output of the command
bool cmd_append_null(cmd_t *cmd, ...);
const char *cmd_render(cmd_t *cmd); // the responsibility of freeing the char * ptr returned by cmd_render after use is of the user; freeing is done using free(str)

#endif