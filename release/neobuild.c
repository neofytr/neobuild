#include "neobuild.h"
#include "strix/header/strix.h"

// for pipe
#include <unistd.h>

// for wait
#include <sys/wait.h>

// for bool
#include <stdbool.h>

// for errno
#include <errno.h>

// for file stats
#include <sys/stat.h>

#define MAX_TEMP_STRLEN (2048)

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

neoconfig_t *neo_parse_config(const char *config_file_path, size_t *config_num)
{
    if (!config_file_path || !config_num)
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[%s] Arguments invalid", __func__);
        NEO_LOG(ERROR, msg);
        return NULL;
    }

#define INIT_CONFIG_SIZE 128

    neoconfig_t *config = (neoconfig_t *)malloc(sizeof(neoconfig_t) * INIT_CONFIG_SIZE);
    if (!config)
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[%s] Config array allocation failed: %s", __func__, strerror(errno));
        NEO_LOG(ERROR, msg);
        return NULL;
    }

    size_t curr_cap = INIT_CONFIG_SIZE;
#undef INIT_CONFIG_SIZE

    size_t curr_index = 0;
    strix_t *file = conv_file_to_strix(config_file_path);
    if (!file)
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[%s] File conversion to strix failed", __func__);
        NEO_LOG(ERROR, msg);
        free(config);
        return NULL;
    }

    strix_arr_t *arr = strix_split_by_delim(file, ';');
    if (!arr)
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[%s] Config parsing failed", __func__);
        NEO_LOG(ERROR, msg);
        free(config);
        strix_free(file);
        return NULL;
    }

    for (size_t index = 0; index < arr->len; index++)
    {
        strix_t *conf = arr->strix_arr[index];

        size_t key_len = 0;
        size_t value_len = 0;

        bool found = false;

        for (size_t counter = 0; counter < conf->len; counter++)
        {
            if (conf->str[counter] == '=')
            {
                found = true;
            }

            if (found)
            {
                value_len++;
            }
            else
            {
                key_len++;
            }
        }

        if (!found)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[%s] Invalid Config-Value pair: " STRIX_FORMAT, __func__, STRIX_PRINT(conf));
            NEO_LOG(ERROR, msg);
            continue;
        }

        char *config_ = (char *)malloc(key_len + 1); // + 1 for null byte
        if (!config_)
        {
            for (size_t i = 0; i < curr_index; i++)
            {
                free(config[i].config);
                free(config[i].value);
            }
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[%s] Config-Value pair allocation failed: %s", __func__, strerror(errno));
            NEO_LOG(ERROR, msg);
            free(config);
            strix_free(file);
            strix_free_strix_arr(arr);
            return NULL;
        }

        char *value = (char *)malloc(value_len + 1);
        if (!value)
        {
            for (size_t i = 0; i < curr_index; i++)
            {
                free(config[i].config);
                free(config[i].value);
            }
            char msg[MAX_TEMP_STRLEN];
            free(config_);
            snprintf(msg, sizeof(msg), "[%s] Config-Value pair allocation failed: %s", __func__, strerror(errno));
            NEO_LOG(ERROR, msg);
            free(config);
            strix_free(file);
            strix_free_strix_arr(arr);
            return NULL;
        }

        memcpy(config_, conf->str, key_len - 1); // -1 to exclude '='
        config_[key_len - 1] = 0;

        memcpy(value, conf->str + key_len, value_len - 1); // Start after '='
        value[value_len - 1] = 0;

        if (curr_index >= curr_cap)
        {
            size_t new_cap = curr_cap * 2;
            neoconfig_t *temp = (neoconfig_t *)realloc(config, sizeof(neoconfig_t) * new_cap);
            if (!temp)
            {
                char msg[MAX_TEMP_STRLEN];
                snprintf(msg, sizeof(msg), "[%s] Config array reallocation failed: %s", __func__, strerror(errno));
                NEO_LOG(ERROR, msg);
                free(config_);
                free(value);

                // free already allocated entries
                for (size_t i = 0; i < curr_index; i++)
                {
                    free(config[i].config);
                    free(config[i].value);
                }
                free(config);
                strix_free(file);
                strix_free_strix_arr(arr);
                return NULL;
            }
            config = temp;
            curr_cap = new_cap;
        }

        config[curr_index].config = config_;
        config[curr_index].value = value;
        curr_index++;
    }

    *config_num = curr_index;

    strix_free_strix_arr(arr);
    strix_free(file);

    if (!curr_index)
    {
        free(config);
        return NULL;
    }

    neoconfig_t *final_config = (neoconfig_t *)realloc(config, sizeof(neoconfig_t) * curr_index);
    if (!final_config)
    {
        // if realloc fails, the original block is still valid
        return config;
    }

    return final_config;
}

bool neo_mkdir(const char *dir_path, mode_t dir_mode)
{
    if (!dir_path)
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[%s] Argument dir_path is invalid", __func__);
        NEO_LOG(ERROR, msg);
        return false;
    }

    if (dir_mode)
    {
        if (mkdir(dir_path, 0777) == -1)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[%s] Creating dir %s failed", __func__, dir_path);
            NEO_LOG(ERROR, msg);
            return false;
        }
    }
    else
    {
        if (mkdir(dir_path, dir_mode) == -1)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[%s] Creating dir %s failed", __func__, dir_path);
            NEO_LOG(ERROR, msg);
            return false;
        }
    }

    return true;
}

bool neorebuild(const char *build_file_c, char **argv)
{
    if (!argv)
        return true;

    char **temp = argv;
    temp++;
    while (*temp)
    {
        if (!strcmp(*temp, "--no-rebuild"))
            return true;
        temp++;
    }

    if (!build_file_c)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neorebuild] Build file pointer is NULL");
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    struct stat build_file_c_stat;
    if (stat(build_file_c, &build_file_c_stat))
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neorebuild] Failed getting file stats for %s: %s", build_file_c, strerror(errno));
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    size_t build_file_len = strlen(build_file_c);
    char *build_file = (char *)malloc((build_file_len + 1) * sizeof(char));
    if (!build_file)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neorebuild] Memory allocation failed: %s", strerror(errno));
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    size_t index = 0;
    while (index < build_file_len)
    {
        if (index < build_file_len - 1 && build_file_c[index] == '.' && build_file_c[index + 1] == 'c')
            break;

        build_file[index] = build_file_c[index];
        index++;
    }

    build_file[index] = 0;

    struct stat build_file_stat;
    if (stat(build_file, &build_file_stat))
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neorebuild] Failed getting file stats for %s: %s", build_file, strerror(errno));
        NEO_LOG(ERROR, error_msg);
        free(build_file);
        return false;
    }

    if (build_file_stat.st_mtime < build_file_c_stat.st_mtime)
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[neorebuild] The build file %s was modified since it was last built", build_file_c);
        NEO_LOG(INFO, msg);

        snprintf(msg, sizeof(msg), "[neorebuild] Rebuilding %s", build_file_c);
        NEO_LOG(INFO, msg);

        char cmd[MAX_TEMP_STRLEN];
        snprintf(cmd, sizeof(cmd), "./buildneo %s", build_file_c);
        NEO_LOG(INFO, cmd);

        if (system(cmd) == -1)
        {
            snprintf(msg, sizeof(msg), "[neorebuild] Rebuilding %s failed: %s", build_file_c, strerror(errno));
            NEO_LOG(ERROR, msg);
            snprintf(msg, sizeof(msg), "[neorebuild] Running the old version of %s", build_file);
            NEO_LOG(INFO, msg);
            free(build_file);
            return false;
        }

        snprintf(msg, sizeof(msg), "[neorebuild] Running the new version of %s and exiting the current running version", build_file);
        NEO_LOG(INFO, msg);

        neocmd_t *neo = neocmd_create(SH);
        if (!neo)
        {
            snprintf(msg, sizeof(msg), "[neorebuild] Failed running the new version of %s; Continuing with the current running version: %s", build_file, strerror(errno));
            NEO_LOG(ERROR, msg);
            free(build_file);
            return false;
        }

        neocmd_append(neo, "./neo --no-rebuild");

        char **arg_ptr = argv;
        arg_ptr++; // skip program name
        while (*arg_ptr)
        {
            neocmd_append(neo, *arg_ptr);
            arg_ptr++;
        }

        if (!neocmd_run_sync(neo, NULL, NULL, false))
        {
            snprintf(msg, sizeof(msg), "[neorebuild] Failed running the new version of %s; Continuing with the current running version: %s", build_file, strerror(errno));
            NEO_LOG(ERROR, msg);
            free(build_file);
            neocmd_delete(neo);
            return false;
        }

        free(build_file);
        neocmd_delete(neo);
        exit(EXIT_SUCCESS);
        return true; // never reached
    }
    else
    {
        char msg[MAX_TEMP_STRLEN];
        snprintf(msg, sizeof(msg), "[neorebuild] No rebuild required for %s (not modified)", build_file_c);
        NEO_LOG(INFO, msg);
    }

    free(build_file);
    return true;
}

const char *neocmd_render(neocmd_t *neocmd)
{
    if (!neocmd || !neocmd->args)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_render] Invalid neocmd or args pointer");
        NEO_LOG(ERROR, error_msg);
        return NULL;
    }

    strix_t *strix = strix_create_empty();
    if (!strix)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_render] Failed to create empty strix");
        NEO_LOG(ERROR, error_msg);
        return NULL;
    }

    dyn_arr_t *arr = neocmd->args;
    int64_t last = arr->last_index;

    for (int64_t index = 0; index <= last; index++)
    {
        strix_t *temp;
        if (!dyn_arr_get(arr, index, &temp))
        {
            char error_msg[MAX_TEMP_STRLEN];
            snprintf(error_msg, sizeof(error_msg), "[neocmd_render] Failed to get item at index %ld", index);
            NEO_LOG(ERROR, error_msg);
            strix_free(strix);
            return NULL;
        }

        if (!strix_concat(strix, temp))
        {
            char error_msg[MAX_TEMP_STRLEN];
            snprintf(error_msg, sizeof(error_msg), "[neocmd_render] Failed to concatenate strix at index %ld", index);
            NEO_LOG(ERROR, error_msg);
            strix_free(strix);
            return NULL;
        }

        if (!strix_append(strix, " "))
        {
            char error_msg[MAX_TEMP_STRLEN];
            snprintf(error_msg, sizeof(error_msg), "[neocmd_render] Failed to append space after index %ld", index);
            NEO_LOG(ERROR, error_msg);
            strix_free(strix);
            return NULL;
        }
    }

    char *str = strix_to_cstr(strix);
    if (!str)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_render] Failed to convert strix to C string");
        NEO_LOG(ERROR, error_msg);
        strix_free(strix);
        return NULL;
    }

    strix_free(strix);
    return (const char *)str;
}

bool neoshell_wait(pid_t pid, int *status, int *code, bool should_print)
{
    // check for invalid arguments
    if (pid < 0)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neoshell_wait] Invalid pid: %d", pid);
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    siginfo_t info;
    // wait for the child process with the given pid to exit or stop
    if (waitid(P_PID, (id_t)pid, &info, WEXITED | WSTOPPED) == -1)
    {
        if (should_print)
        {
            char error_msg[MAX_TEMP_STRLEN];
            snprintf(error_msg, sizeof(error_msg), "[neoshell_wait] waitid on pid %d failed: %s", pid, strerror(errno));
            NEO_LOG(ERROR, error_msg);
        }
        return false;
    }

    // store the termination reason and status
    if (code)
    {
        *code = info.si_code;
    }

    if (status)
    {
        *status = info.si_status;
    }

    // check how the child process terminated
    switch (info.si_code)
    {
    case CLD_EXITED:
        // child exited normally, store the exit status
        if (should_print)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[neoshell_wait] shell process %d exited normally with status %d", pid, info.si_status);
            NEO_LOG(INFO, msg);
        }
        break;

    case CLD_KILLED:
        // child was killed by a signal
        if (should_print)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[neoshell_wait] shell process %d was killed by signal %d", pid, info.si_status);
            NEO_LOG(ERROR, msg);
        }
        break;

    case CLD_DUMPED:
        // child was killed by a signal and dumped core
        if (should_print)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[neoshell_wait] shell process %d was killed by signal %d (core dumped)", pid, info.si_status);
            NEO_LOG(ERROR, msg);
        }
        break;

    case CLD_STOPPED:
        // child was stopped by a signal
        if (should_print)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[neoshell_wait] shell process %d was stopped by signal %d", pid, info.si_status);
            NEO_LOG(ERROR, msg);
        }
        break;

    case CLD_TRAPPED:
        // traced child has trapped (e.g., during debugging)
        if (should_print)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[neoshell_wait] shell process %d was trapped by signal %d (traced child)", pid, info.si_status);
            NEO_LOG(ERROR, msg);
        }
        break;

    default:
        // unknown or unexpected termination reason
        if (should_print)
        {
            char msg[MAX_TEMP_STRLEN];
            snprintf(msg, sizeof(msg), "[neoshell_wait] shell process %d terminated in an unknown way (si_code: %d, si_status: %d)",
                     pid, info.si_code, info.si_status);
            NEO_LOG(ERROR, msg);
        }
        return false;
    }

    return true;
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
pid_t neocmd_run_async(neocmd_t *neocmd)
{
    // returns -1 if an error occurred

    if (!neocmd)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async] Invalid neocmd pointer");
        NEO_LOG(ERROR, error_msg);
        return -1;
    }

    const char *command = neocmd_render(neocmd);
    if (!command)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async] Failed to render command");
        NEO_LOG(ERROR, error_msg);
        return -1;
    }

    char msg[512];
    snprintf(msg, sizeof(msg), "[neocmd_run_async] %s", command);
    NEO_LOG(INFO, msg); // display the command being run by the newly created shell

    pid_t child = fork();

    if (child == -1)
    {
        // no child process is created
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async] Child process could not be forked: %s", strerror(errno));
        NEO_LOG(ERROR, error_msg);
        free((void *)command);
        return -1;
    }
    else if (!child)
    {
        // child process
        switch (neocmd->shell)
        {
        case BASH:
        {
            char *argv[4] = {"/bin/bash", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the neocmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/bash", argv) == -1)
            {
                char error_msg[MAX_TEMP_STRLEN];
                snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async:child] Child shell could not be executed: %s", strerror(errno));
                NEO_LOG(ERROR, error_msg);
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        case SH:
        {
            char *argv[4] = {"/bin/sh", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the neocmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/sh", argv) == -1)
            {
                char error_msg[MAX_TEMP_STRLEN];
                snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async:child] Child shell could not be executed: %s", strerror(errno));
                NEO_LOG(ERROR, error_msg);
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        case DASH:
        {
            char *argv[4] = {"/bin/dash", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the neocmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/dash", argv) == -1)
            {
                char error_msg[MAX_TEMP_STRLEN];
                snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async:child] Child shell could not be executed: %s", strerror(errno));
                NEO_LOG(ERROR, error_msg);
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        default:
        {
            // execute BASH in the default case
            char *argv[4] = {"/bin/bash", "-c", (char *)command, NULL}; // NULL marks the end of the argv array
            // the output of the command will be displayed in the shell running the neocmd_run function
            // since the stdout of the child and parent refer to the same open file description
            if (execv("/bin/bash", argv) == -1)
            {
                char error_msg[MAX_TEMP_STRLEN];
                snprintf(error_msg, sizeof(error_msg), "[neocmd_run_async:child] Child shell could not be executed: %s", strerror(errno));
                NEO_LOG(ERROR, error_msg);
                free((void *)command);
                return EXIT_FAILURE;
            }
        }
        }
    }
    else
    {
        // parent process; immediately return the pid_t
        free((void *)command);
        return child;
    }

    // would not be reached
    free((void *)command);
    return -1;
}

bool neocmd_run_sync(neocmd_t *neocmd, int *status, int *code, bool print_status_desc)
{
    // the parent and child share the same open file descriptions and file descriptors
    // for stdin, stdout, and stderr

    pid_t child = neocmd_run_async(neocmd);
    if (child == -1)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_run_sync] Failed to run command asynchronously");
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    neoshell_wait(child, status, code, print_status_desc);
    return true;
}

#undef READ_END
#undef WRITE_END

neocmd_t *neocmd_create(neoshell_t shell)
{
    neocmd_t *neocmd = (neocmd_t *)malloc(sizeof(neocmd_t));
    if (!neocmd)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_create] Failed to allocate memory for neocmd");
        NEO_LOG(ERROR, error_msg);
        return NULL;
    }

#define MIN_ARG_NUM 16
    neocmd->args = dyn_arr_create(MIN_ARG_NUM, sizeof(strix_t *), NULL);
#undef MIN_ARG_NUM
    if (!neocmd->args)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_create] Failed to create dynamic array for arguments");
        NEO_LOG(ERROR, error_msg);
        free(neocmd);
        return NULL;
    }
    neocmd->shell = shell;

    return neocmd;
}

bool neocmd_delete(neocmd_t *neocmd)
{
    if (!neocmd || !neocmd->args)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_delete] Invalid neocmd or args pointer");
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    cleanup_arg_array(neocmd->args);

    dyn_arr_free(neocmd->args);
    free((void *)neocmd);

    return true;
}

bool neocmd_append_null(neocmd_t *neocmd, ...)
{
    if (!neocmd || !neocmd->args)
    {
        char error_msg[MAX_TEMP_STRLEN];
        snprintf(error_msg, sizeof(error_msg), "[neocmd_append_null] Invalid neocmd or args pointer");
        NEO_LOG(ERROR, error_msg);
        return false;
    }

    va_list args;
    va_start(args, neocmd); // the variadic arguments start after the parameter neocmd; initialize the list with the last static arguments
    dyn_arr_t *neocmd_args = neocmd->args;

    const char *arg = va_arg(args, const char *);
    while (arg)
    {
        strix_t *arg_strix = strix_create(arg);
        if (!arg_strix)
        {
            char error_msg[MAX_TEMP_STRLEN];
            snprintf(error_msg, sizeof(error_msg), "[neocmd_append_null] Failed to create strix for argument: %s", arg);
            NEO_LOG(ERROR, error_msg);
            cleanup_arg_array(neocmd_args);
            va_end(args);
            return false;
        }

        if (!dyn_arr_append(neocmd_args, &arg_strix))
        {
            char error_msg[MAX_TEMP_STRLEN];
            snprintf(error_msg, sizeof(error_msg), "[neocmd_append_null] Failed to append argument to array: %s", arg);
            NEO_LOG(ERROR, error_msg);
            APPEND_CLEANUP(neocmd_args);
        }
        arg = va_arg(args, const char *);
    }

    va_end(args); // finished extracting all variadic arguments; cleanup
    return true;
}

#undef MAX_TEMP_STRLEN