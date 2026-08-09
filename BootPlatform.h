/**
 * @file    BootPlatform.h
 * @brief   Platform / BSP abstraction for the Bootloader.
 *
 * This module hides chip-specific environment setup from the Bootloader
 * core. The core consumes these services only through the global ops
 * table @c g_Platform, so porting to another MCU family only requires a
 * new template implementation, e.g. BootPlatform_stm32f1_Template.c.
 *
 * Responsibilities:
 *   - Bring up the runtime environment (HAL init, system clock, debug
 *     UART, ...).
 *   - Feed the independent watchdog during long operations (CRC scan,
 *     Flash erase / write).
 *   - Emit debug / log messages.
 *   - Tear the environment down before jumping to the APP.
 *
 * The implementation must be registered with BootPlatform_Register()
 * before Cus_Bootloader_Init() runs (see Cus_Bootloader_InstallFunctions).
 */
#ifndef __BOOT_PLATFORM_H__
#define __BOOT_PLATFORM_H__

#include <stdint.h>
#include "BootloaderConf.h"


/* ********************************************* */
typedef void (*BootPlatform_EnvInitFn)( void );
typedef void (*BootPlatform_PrepareJumpFn)( void );
typedef void (*BootPlatform_FeedDGFn)( void );
typedef void (*BootPlatform_DelayMsFn)( uint32_t ms );
typedef void (*BootPlatform_DebugFn)( const char *msg, uint32_t err_code );


/*
 * Ops member conventions:
 *   - REQUIRED: called unconditionally by the core; must be implemented
 *     and non-NULL. The core BL_ASSERTs these at startup.
 *   - OPTIONAL: may be NULL; the core null-checks the pointer before
 *     calling, so a disabled feature simply registers NULL.
 */
typedef struct 
{
    /**
     * REQUIRED: must be implemented and non-NULL.
     * Platform bring-up: HAL_Init, system clock configuration, debug UART
     * setup, etc. Invoked by the core as the very first step of
     * Cus_Bootloader_Init(), before any other Bootloader service is used.
     * Implementation must leave the platform in a fully usable state.
     */
    BootPlatform_EnvInitFn      Init;

    /**
     * OPTIONAL: may be NULL (e.g. when USE_DG is disabled or the board
     * has no watchdog).
     * Feed the independent watchdog. Invoked periodically by the core
     * during long-running operations (CRC scan, Flash erase / write) and
     * inside the BL_ASSERT hang loop when USE_DG is enabled.
     */
    BootPlatform_FeedDGFn       FeedDg;

    /**
     * OPTIONAL: may be NULL (e.g. when USE_DEBUG is disabled).
     * Debug output sink. Receives a message string and an error code.
     * Invoked via BL_Log() / BL_LogF() when USE_DEBUG is enabled.
     * Implementation shall forward the message to the chosen debug
     * channel (UART, SWO, ...).
     */
    BootPlatform_DebugFn        LogOut;

    /**
     * Pre-jump hook. OPTIONAL: may be NULL.
     * Invoked by the core right before the application jump (main.c),
     * when registered, to let the user perform ANY last-minute
     * operations, e.g. disable SysTick, clear pending interrupts,
     * de-init peripherals, feed the watchdog one last time. The core
     * performs __disable_irq() and the VTOR switch AFTER this hook
     * returns, so the user does not need to repeat them here.
     */
    BootPlatform_PrepareJumpFn  PrepareJump;

    /**
     * Millisecond delay. REQUIRED: must be implemented and non-NULL.
     * Used by the core for timed watchdog feeding inside the BL_ASSERT
     * hang loop (via BL_DelayMs). Implementation may rely on HAL_Delay,
     * DWT or any other tick source; must keep running while interrupts
     * are enabled.
     */
    BootPlatform_DelayMsFn      DelayMs;

} BootPlatform_Ops_t;


extern const BootPlatform_Ops_t *g_Platform;
/* ********************************************* */


void BootPlatform_Register( const BootPlatform_Ops_t *ops );

#endif /* __BOOT_PLATFORM_H__ */