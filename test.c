#include "neobuild.h"
#include <stdio.h>

int main()
{
    cmd_t *cmd = cmd_create(BASH);
    cmd_append(cmd, "clang", "-Wall", "test.c", "-o", "test");
    const char *str = cmd_render(cmd);
    fprintf(stdout, "%s\n", str);
    free((char *)str);
    return EXIT_SUCCESS;
}