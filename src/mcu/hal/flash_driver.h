/**
 * @file flash_driver.h
 * @brief Flash memory driver for config storage
 */

#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../common/error_codes.h"

#define FLASH_CONFIG_ADDR   0x08030000UL
#define FLASH_CONFIG_SIZE   4096U  /* 4 KB sector */
#define FLASH_PAGE_SIZE     4096U

sentinel_err_t flash_init(void);
sentinel_err_t flash_erase_page(uint32_t addr);
sentinel_err_t flash_write(uint32_t addr, const uint8_t *data, size_t len);
sentinel_err_t flash_read(uint32_t addr, uint8_t *data, size_t len);

#endif /* FLASH_DRIVER_H */
