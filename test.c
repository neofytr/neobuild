#include "release/neobuild.h"
#include <stdio.h>

int main()
{
    neocmd_t *cmd = cmd_create(BASH);
    neocmd_append(cmd, "clang", "-Wall", LABEL_WITH_SPACES("main hop.c"), "-o", "main", "&& ./main");
    int status, code;
    neocmd_run_sync(cmd, NULL, NULL, false);
    return EXIT_SUCCESS;
}