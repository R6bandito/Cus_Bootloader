/**
 * @file    BootFlashPort.h
 * @brief   Flash backend abstraction for the Bootloader.
 *
 * This module isolates ALL Flash-storage operations from the Bootloader
 * core logic. The core (Bootloader.c / main.c) accesses the storage only
 * through the global ops table @c g_BootFlash, so the underlying medium
 * (on-chip Flash, external SPI Flash, or a KV/record manager such as
 * Cus_Flash) can be swapped without touching the core.
 *
 * Responsibilities:
 *   - Initialize the storage backend (latency calibration, KV manager
 *     bring-up, ...).
 *   - Persist, read back and clear the IAP update request record.
 *   - Maintain the A/B slot-flag records (read latest / flip).
 *
 * NOTE: firmware erase / write is NOT part of this interface -- in the
 * A/B flow the firmware is written by the user side (APP / DFU APP),
 * the Bootloader core only verifies the target slot and flips the flag.
 * DFU-side Flash writes are implemented by the user in the DFU APP
 * (directly on Cus_Flash), independent of this core interface.
 *
 * A concrete backend is supplied by a template implementation, e.g.
 * BootFlashPort_Template_kv.c (Cus_Flash based). It must be registered
 * with BootFlash_Register() before Cus_Bootloader_Init() runs.
 */
#ifndef __BOOT_FLASH_PORT_H__
#define __BOOT_FLASH_PORT_H__

#include <stdint.h>
#include <stdbool.h>
#include "BootloaderConf.h"
#include "IAP_Protocol.h"

typedef int  (*BootFlash_InitFn)  ( void );
typedef bool (*BootFlash_ReadFn)  ( uint8_t *buf, uint32_t size );
typedef bool (*BootFlash_ClearIAPFn)( void );  
typedef bool (*BootFlash_ReadSlotFn)( SlotFlag_Rec_t *out );
typedef bool (*BootFlash_FlipSlotFn)( const SlotFlag_Rec_t *rec );


/*
 * Ops member conventions:
 *   - REQUIRED: called unconditionally by the core; must be implemented
 *     and non-NULL. The core BL_ASSERTs these at startup.
 *   - OPTIONAL: may be NULL; the core null-checks the pointer before
 *     calling (Init is currently the only OPTIONAL member).
 */
typedef struct {
    /**
     * OPTIONAL: may be NULL when the backend needs no initialization.
     * Initialize the storage backend.
     * Invoked by the core during Cus_Bootloader_Init() (null-checked),
     * before any other Flash operation. Implementation must bring the
     * backend to a fully usable state (e.g. Cus_FlashMgr_Init, latency
     * calibration).
     * @return 0 on success, negative error code on failure.
     */
    BootFlash_InitFn   Init;

    /**
     * REQUIRED: must be implemented and non-NULL.
     * Read the latest IAP update request record into @c buf.
     * The record layout is IAP_Info_t (magic / app_size / CRC32). Used by
     * the core to detect a pending update and to verify its CRC.
     * @return true if a valid request record was found and copied.
     */
    BootFlash_ReadFn   ReadIAP;

    /**
     * REQUIRED: must be implemented and non-NULL.
     * Clear / invalidate the IAP update request record.
     * Called by the core after a successful update, or when a pending
     * request fails CRC verification and must be discarded.
     * @return true on success.
     */
    BootFlash_ClearIAPFn  ClearIAP;

	/* Read the latest valid slot-flag record (backend lookup +
		magic/CRC validation inside). MANDATORY: called unconditionally
		by the core (asserted at startup). Returns false when no valid
		record exists (caller defaults to A). */
	BootFlash_ReadSlotFn  ReadSlot;

	/* Persist one slot-flag record (see IAP_Protocol.h SlotFlag_Rec_t).
		The caller (Bootloader core) builds the record: magic / active /
		seq / crc are already finalized. The implementation only decides
		HOW to store it: erase-page-then-write, or append into the log
		area with wear leveling. Store the record verbatim; do not modify
		it. MANDATORY (asserted at startup).
		@return true only when the record is durable. */
	BootFlash_FlipSlotFn  FlipSlot;

} BootFlash_Ops_t;


extern const BootFlash_Ops_t *g_BootFlash;

void BootFlash_Register( const BootFlash_Ops_t *ops );

#endif

