#include "release/neobuild.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    neorebuild("neo.c", argv);
    
    neocmd_t *cmd = neocmd_create(BASH);
    neocmd_append(cmd, "clang", "-Wall", "temporary.c", "-o", "main", "&& ./main");
    int status, code;
    neocmd_run_sync(cmd, NULL, NULL, false);
    return EXIT_SUCCESS;
}