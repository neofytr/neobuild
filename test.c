#include "neobuild/neobuild.h"
#include <stdio.h>

int main()
{
    cmd_t *cmd = cmd_create(BASH);
    cmd_append(cmd, "clang", "-Wall", LABEL_WITH_SPACES("main hop.c"), "-o", "main", "&& ./main");
    int status, code;
    cmd_run_sync(cmd, NULL, NULL, false);
    return EXIT_SUCCESS;
}