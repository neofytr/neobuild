#ifndef NEOBUILD_H
#define NEOBUILD_H

#include "dynarr/inc/dynarr.h"
#include <stdint.h>
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

cmd_t *cmd_create(shell_t shell);
bool cmd_delete(cmd_t *cmd);
bool cmd_append(cmd_t *cmd, ...);

#endif