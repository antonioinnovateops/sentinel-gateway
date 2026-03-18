/**
 * @file flash_driver.c
 * @brief Internal flash driver for STM32U575
 *
 * For QEMU emulation, we simulate flash in RAM since QEMU
 * doesn't fully emulate STM32 flash controller.
 */

#include "flash_driver.h"
#include "stm32u575.h"
#include <string.h>

/* Simulated config flash in RAM (for QEMU compatibility) */
static uint8_t g_simulated_flash[FLASH_CONFIG_SIZE] __attribute__((aligned(4)));
static bool g_flash_initialized = false;

sentinel_err_t flash_init(void)
{
    /* Initialize simulated flash to 0xFF (erased state) */
    (void)memset(g_simulated_flash, 0xFF, sizeof(g_simulated_flash));
    g_flash_initialized = true;
    return SENTINEL_OK;
}

sentinel_err_t flash_erase_page(uint32_t addr)
{
    if (!g_flash_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    if (addr != FLASH_CONFIG_ADDR) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    (void)memset(g_simulated_flash, 0xFF, FLASH_CONFIG_SIZE);
    return SENTINEL_OK;
}

sentinel_err_t flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!g_flash_initialized || (data == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t offset = addr - FLASH_CONFIG_ADDR;
    if ((offset + len) > FLASH_CONFIG_SIZE) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    (void)memcpy(&g_simulated_flash[offset], data, len);
    return SENTINEL_OK;
}

sentinel_err_t flash_read(uint32_t addr, uint8_t *data, size_t len)
{
    if (!g_flash_initialized || (data == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t offset = addr - FLASH_CONFIG_ADDR;
    if ((offset + len) > FLASH_CONFIG_SIZE) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    (void)memcpy(data, &g_simulated_flash[offset], len);
    return SENTINEL_OK;
}
