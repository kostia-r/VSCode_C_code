#include <stdio.h>
#include "module.h"
#include "SysCall_learning.h"

int main(void)
{
    print_hello();

    syscall_file_manipulations();
    return 0;
}
