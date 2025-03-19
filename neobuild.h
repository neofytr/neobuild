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
bool cmd_append_null(cmd_t *cmd, ...);

#endif