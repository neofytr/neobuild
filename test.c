#include "neobuild.h"

int main()
{
    cmd_t *cmd = cmd_create(BASH);
    cmd_append(cmd, "clang", "-Wall", "test.c", "-o", "test");
    return EXIT_SUCCESS;
}