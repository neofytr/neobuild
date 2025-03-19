#include "neobuild.h"
#include "strix/header/strix.h"

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
    if (!cmd && !cmd->args)
    {
        return NULL;
    }

    strix_t *strix = strix_create("");
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
    }
}

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

        if (!strix_append(arg_strix, " ")) // we append a space at the end of each of the arguments
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