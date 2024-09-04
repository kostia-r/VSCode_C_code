#include "module.h"

#include <stdio.h>

void print_hello(void)
{
#ifdef DEBUG
    printf("Hello, World in debug!\n");
#elif RELEASE
    printf("Hello, World in Release!\n");
#endif
}
