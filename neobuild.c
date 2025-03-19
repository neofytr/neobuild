#include "neobuild.h"

cmd_t *create_cmd(shell_t shell)
{
    cmd_t *cmd = (cmd_t *)malloc(sizeof(cmd_t));
    if (!cmd)
    {
        return NULL;
    }

#define MIN_ARG_NUM 8
    cmd->args = dyn_arr_create(MIN_ARG_NUM, sizeof(char *), NULL);
#undef MIN_ARG_NUM


}