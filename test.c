#include "neobuild.h"
#include <stdio.h>

int main()
{
    cmd_t *cmd = cmd_create(BASH);
    cmd_append(cmd, "clang", "-Wall", "main.c", "-o", "main", "&& ./main");
    int code;
    cmd_run(cmd, &code);
    return EXIT_SUCCESS;
}