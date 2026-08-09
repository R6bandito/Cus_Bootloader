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
 *   - Erase / write / verify the APP download region.
 *   - Persist, read back and clear the IAP update request record.
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
typedef int  (*BootFlash_EraseFn) ( uint32_t addr, uint32_t size );
typedef int  (*BootFlash_WriteFn) ( uint32_t addr, const uint8_t *data, uint32_t size );
typedef bool (*BootFlash_ReadFn)  ( uint8_t *buf, uint32_t size );
typedef bool (*BootFlash_ClearIAPFn)( void );  
typedef bool (*BootFlash_VerifyFn)( uint32_t addr, const uint8_t *data, uint32_t size );

#if (USE_AB_SLOT)
	typedef bool (*BootFlash_ReadSlotFn)( SlotFlag_Rec_t *out );
	typedef bool (*BootFlash_FlipSlotFn)( const SlotFlag_Rec_t *rec );
#endif


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
     * Erase the region [addr, addr + size).
     * Implementation shall round the range up to the next page/sector
     * boundary so that partially covered pages are fully erased. Used by
     * the core to wipe the APP region before writing new firmware.
     * @return 0 on success, negative error code on failure.
     */
    BootFlash_EraseFn  Erase;

    /**
     * REQUIRED: must be implemented and non-NULL.
     * Write @c size bytes from @c data to @c addr.
     * Implementation must validate inputs (NULL data, zero size) and
     * handle alignment internally. Used to write firmware in packets.
     * @return 0 on success, negative error code on failure.
     */
    BootFlash_WriteFn  Write;

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

    /**
     * REQUIRED: must be implemented and non-NULL.
     * Byte-by-byte compare the Flash region [addr, addr + size) against
     * @c data. Used by the core to verify the written firmware before
     * jumping to the APP.
     * @return true if the region exactly matches @c data.
     */
    BootFlash_VerifyFn Verify;

	#if (USE_AB_SLOT)
		/* Read the latest valid slot-flag record (backend lookup +
			magic/CRC validation inside). NULL-safe: the core falls back
			to "slot A active" when this is NULL or returns false. */
		BootFlash_ReadSlotFn  ReadSlot;

		/* Persist one slot-flag record (see IAP_Protocol.h SlotFlag_Rec_t).
			The caller (Bootloader core) builds the record: magic / active /
			seq / crc are already finalized. The implementation only decides
			HOW to store it: erase-page-then-write, or append into the log
			area with wear leveling. Store the record verbatim; do not modify
			it. @return true only when the record is durable. */
		BootFlash_FlipSlotFn  FlipSlot;
	#endif

} BootFlash_Ops_t;


extern const BootFlash_Ops_t *g_BootFlash;

void BootFlash_Register( const BootFlash_Ops_t *ops );

#endif