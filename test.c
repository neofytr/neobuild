#include "neobuild.h"
#include <stdio.h>

int main()
{
    cmd_t *cmd = cmd_create(BASH);
    cmd_append(cmd, "clang", "-Wall", "test.c", "-o", "test");
    char *str = cmd_render(cmd);
    fprintf(stdout, "%s\n", str);
    free(str);
    return EXIT_SUCCESS;
}