/*
 * IAP_Protocol.h -- SHARED CONTRACT between the Bootloader project and
 * the APP project (and the DFU APP, if enabled). Everything here is
 * visible to both sides: memory layout, update request format, A/B
 * slot records and checksums. Bootloader-private configuration lives
 * in BootloaderConf.h instead.
 */
#ifndef __CUS_IAP_PROTOCAL_H__
#define __CUS_IAP_PROTOCAL_H__


#include <stdint.h>



typedef struct Bootloader_info
{
	uint16_t magic_word;
	uint16_t version;
	uint32_t app_size;
	uint32_t CRC32;

} IAP_Info_t;


typedef struct 
{
	uint32_t magic;    /* SLOT_REC_MAGIC: valid-record marker */
	uint32_t active;   /* SLOT_FLAG_MAGIC_A or SLOT_FLAG_MAGIC_B */
	uint32_t seq;      /* monotonic counter; highest = newest */
	uint32_t crc;      /* checksum of the previous 3 words */

} SlotFlag_Rec_t;


/* -------------- Core Config ------------------- */
/*
 * @brief Bootloader memory layout configuration (ZET6 example, 512KB).
 * @note  Adjust these addresses/sizes for your target MCU (e.g. the C8T6
 *        layout is defined separately and much smaller).
 *
 * Actual layout (values below):
 *
 *   [0x08000000] Bootloader          64KB   must match the linker script
 *   [0x08010000] APP Slot A         200KB   active slot by default
 *   [0x08042000] IAP Manager          8KB   Cus_FlashMgr: IAP_Info_t request records
 *   [0x08044000] APP Slot B         200KB   inactive slot (legacy DOWNLOAD region)
 *   [0x08076000] free                2KB
 *   [0x08076800] Slot Flag            2KB   active-slot marker page (A/B section)
 *   [0x08077000] free                4KB
 *   [0x08078000] Resume Manager       8KB   Cus_FlashMgr: power-fail resume records
 *   [0x0807A000] free               24KB
 *   [0x08080000] flash end (512KB)
 *
 * Used: 64+200+8+200+2+2+4+8 = 488KB, leaving 24KB free.
 *
 * Notes:
 * 1. Slot A / Slot B form the A/B dual-slot pair (see the A/B section
 *    below): upgrades only write to the INACTIVE slot, so the running
 *    slot always stays intact.
 * 2. IAP Manager holds the update request (IAP_Info_t: magic / size /
 *    CRC32), written by the APP, read by the Bootloader.
 * 3. Resume Manager holds the power-fail resume record (BootResume).
 * 4. IAP_MAGIC_WORD: known pattern at the head of IAP_Info_t; marks a
 *    valid update request.
 */
/* -------------- Core Config & Define ------------------- */
#define BOOTLOADER_START_ADDRESS          (0x08000000UL)
#define BOOTLOADER_SIZE                   (0x00010000UL)    // 64KB

#define APP_START_ADDRESS                 (0x08010000UL)
#define APP_REGION_SIZE                   (0x00032000UL)    // 200KB

#define DOWNLOAD_START_ADDRESS            (0x08044000UL)
#define DOWNLOAD_REGION_SIZE              (0x00032000UL)    // 200KB

#define IAP_MAGIC_WORD                    (0xAA55UL)  


/* -------------- A/B Dual-Slot OTA (Optional) -------------- */
/*
 * Dual-slot (A/B) OTA layout. Two equal-sized executable partitions:
 * one is ACTIVE (currently running), the other is TARGET (receives the
 * new firmware). Upgrades only touch the TARGET slot; the ACTIVE slot
 * stays untouched, so a failure at any stage (download / erase / write /
 * verify / power loss) simply falls back to the other slot on next boot.
 *
 * The active-slot decision is stored as SlotFlag_Rec_t records in a
 * shared Flash area. The physical storage is managed by the backend:
 * BootFlash ReadSlot / FlipSlot on the Bootloader side, user storage
 * code on the APP side. This header only defines the record format,
 * the checksum and the slot semantics shared by both projects, so the
 * APP can compute where to write the incoming firmware (always the
 * INACTIVE slot) -- no communication channel between the two projects
 * is required.
 *
 * Flag semantics:
 *   - SLOT_FLAG_MAGIC_A  -> slot A active
 *   - SLOT_FLAG_MAGIC_B  -> slot B active
 *   - any other value (e.g. erased 0xFFFFFFFF) -> defaults to slot A
 *
 * NOTE: APP_SLOT_B currently maps to the legacy DOWNLOAD region; the
 * DOWNLOAD_* macros above are kept for backward compatibility.
 *
 * Adjust these addresses/sizes for your target MCU (e.g. C8T6 layout
 * differs from ZET6); the flag page must be page-aligned and must not
 * share a flash page with any other region.
 */
#define APP_SLOT_A_START                  (0x08010000UL)
#define APP_SLOT_A_SIZE                   (0x00032000UL)    // 200KB

#define APP_SLOT_B_START                  (0x08044000UL)
#define APP_SLOT_B_SIZE                   (0x00032000UL)    // 200KB

/* Slot-flag record markers (active field values). */
#define SLOT_REC_MAGIC                    (0x534C4F54UL)              /* "SLOT" */
#define SLOT_FLAG_MAGIC_A                 (0xAA55AA55UL)
#define SLOT_FLAG_MAGIC_B                 (0x55AA55AAUL)


/*
 * NOTE: the slot-flag RECORDS (read/write) are NOT provided by this
 * protocol header. Each side maintains them through its own storage
 * backend:
 *   - Bootloader: the registered BootFlash ReadSlot / FlipSlot ops.
 *   - APP / DFU:  an equivalent implementation over the same region
 *                 (same desc, same SlotFlag_Rec_t layout).
 * ReadSlot must return the LATEST record (highest seq). The APP uses
 * its read path + BootSlot_MagicToAddr() to compute the firmware
 * write target (always the INACTIVE slot).
 */

/* Convert a slot-flag magic value to its partition start address.
   Unknown values fall back to slot A (same default as the core). */
static inline uint32_t BootSlot_MagicToAddr( uint32_t magic )
{
    return ( magic == SLOT_FLAG_MAGIC_B ) ? APP_SLOT_B_START : APP_SLOT_A_START;
}


static inline uint32_t 
SlotFlag_RecCrc( const SlotFlag_Rec_t *rec )
{
	/* 12-byte checksum. */
	uint32_t s = rec->magic ^ rec->active ^ rec->seq;
	return ( s ^ (s >> 16) );
}

#endif /* __CUS_IAP_PROTOCAL_H__ */
