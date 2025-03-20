#include "release/neobuild.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    neorebuild("neo.c", argv);
    size_t len;
    neoconfig_t *config_arr = neo_parse_config_arg(argv, &len);

    for (size_t index = 0; index < len; index++)
    {
        neoconfig_t config = config_arr[index];
        fprintf(stdout, "%s => %s\n", config.key, config.value);
    }

    neocmd_t *cmd = neocmd_create(BASH);
    neocmd_append(cmd, "clang", "-Wall", "temporary.c", "-o", "main", "&& ./main");
    int status, code;
    neocmd_run_sync(cmd, NULL, NULL, false);
    return EXIT_SUCCESS;
}