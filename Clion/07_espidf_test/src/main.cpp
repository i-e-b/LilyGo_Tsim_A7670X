#include <stdio.h>


// Ignore "unused" message for app main, it is the ESP-IDF entry point
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
extern void app_main()
{
    printf("Hello, world\n");
}
#pragma clang diagnostic pop