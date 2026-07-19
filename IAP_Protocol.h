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


/* -------------- Core Config ------------------- */
// @brief Bootloader memory layout configuration.
// @note  Please adjust these addresses and sizes according to your actual MCU flash partitioning.
//        The layout typically consists of: Bootloader | Application | Download (firmware staging) | IAP Info.
//
// 1. BOOTLOADER_START_ADDRESS / SIZE:   Region that holds the bootloader code itself.
//                                       Must match the linker script.
//
// 2. APP_START_ADDRESS / APP_REGION_SIZE:   Region for the main application firmware.
//                                            Bootloader will jump here after update or timeout.
//
// 3. DOWNLOAD_START_ADDRESS / DOWNLOAD_REGION_SIZE:   Temporary storage for incoming firmware image.
//                                                     Bootloader writes received data here before verifying and copying.
//
//
// 4. IAP_MAGIC_WORD:   A known pattern written at the beginning of the IAP info structure to indicate
//                      valid update information (e.g., 0xAA55).
//
// Example for STM32F103ZET6 with 512KB total flash:
//   Bootloader:   0x08000000, 32KB
//   App:          0x08008000, 224KB
//   Download:     0x08040000, 224KB
//   IAP Info:     0x0807F800,  2KB  (last 2KB of 512KB)
//   Total: 32+224+224+2 = 482KB (fit in 512KB, leaving some free space)
/* -------------- Core Config & Define ------------------- */
#define BOOTLOADER_START_ADDRESS          (0x08000000UL)
#define BOOTLOADER_SIZE                   (0x00008000UL)    // 32KB

#define APP_START_ADDRESS                 (0x08008000UL)
#define APP_REGION_SIZE                   (0x00038000UL)    // 224KB

#define DOWNLOAD_START_ADDRESS            (0x08040000UL)
#define DOWNLOAD_REGION_SIZE              (0x00038000UL)    // 224KB
/* 32 + 224 + 224 = 480 KB. */


#define MCU_SRAM_BASE_ADDR                (SRAM_BASE) 
#define MCU_SRAM_SIZE                     (64 * 1024)

#define IAP_MAGIC_WORD                    (0xAA55UL)  
#define BYTES_PER_PACKS                   (1024UL)


#endif /* __CUS_IAP_PROTOCAL_H__ */

