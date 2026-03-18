/**
 * @file main.c
 * @brief Linux gateway entry point
 */

#include "gateway_core.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    sentinel_err_t err = gateway_init();
    if (err != SENTINEL_OK) {
        fprintf(stderr, "Fatal: gateway_init failed (%d)\n", (int)err);
        return EXIT_FAILURE;
    }

    err = gateway_run();
    return (err == SENTINEL_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
