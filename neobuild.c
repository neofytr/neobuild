#include "neobuild.h"

cmd_t *cmd_create(shell_t shell)
{
    cmd_t *cmd = (cmd_t *)malloc(sizeof(cmd_t));
    if (!cmd)
    {
        return NULL;
    }

#define MIN_ARG_NUM 8
    cmd->args = dyn_arr_create(MIN_ARG_NUM, sizeof(char *), NULL);
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

    dyn_arr_free(cmd->args);
    free((void *)cmd);

    return true;
}

