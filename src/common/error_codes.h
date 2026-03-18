/**
 * @file error_codes.h
 * @brief Sentinel Gateway error codes (shared between Linux and MCU)
 * @implements SYS-076 Error Code Definitions
 */

#ifndef SENTINEL_ERROR_CODES_H
#define SENTINEL_ERROR_CODES_H

typedef enum {
    SENTINEL_OK              =   0,
    SENTINEL_ERR_INVALID_ARG =  -1,
    SENTINEL_ERR_OUT_OF_RANGE = -2,
    SENTINEL_ERR_TIMEOUT     =  -3,
    SENTINEL_ERR_COMM        =  -4,
    SENTINEL_ERR_DECODE      =  -5,
    SENTINEL_ERR_ENCODE      =  -6,
    SENTINEL_ERR_FULL        =  -7,
    SENTINEL_ERR_NOT_READY   =  -8,
    SENTINEL_ERR_AUTH        =  -9,
    SENTINEL_ERR_INTERNAL    = -10
} sentinel_err_t;

#endif /* SENTINEL_ERROR_CODES_H */
