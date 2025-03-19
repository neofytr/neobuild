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

bool cmd_append_null(cmd_t *cmd, ...) // the string arguments should not be local variables of the function calling cmd_append
{
    va_list args;
    va_start(args, cmd); // the variadic arguments start after the parameter cmd

    const char *arg = va_arg(args, const char *);
    while (arg)
    {
        if (!dyn_arr_append(cmd->args, &arg)) // the args list just contains the pointers to the memory locations of the argument strings; it doesn't create
                                              // a copy of them; so, the string arguments should not go out of scope at least until the cmd oject supplied is not
                                              // to be utilized anymore
        {
            return false;
        }
        arg = va_arg(args, const char *);
    }

    va_end(args); // finished extracting all variadic arguments
    return true;
}