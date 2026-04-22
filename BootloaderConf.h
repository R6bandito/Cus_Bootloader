#ifndef __BOOTLOADER_CONF_H__
#define __BOOTLOADER_CONF_H__


/* ****************************** */
  #include "stm32f1xx_hal.h"            // 按需更换所使用头文件.
/* ****************************** */

/* -------------- Feature ------------------- */
  #define USE_UTILS_DEBUG       (0)
  #define USE_UTILS_SYSCONF     (1)
/* ------------------------------------------ */

/* -------------- DB Config ------------------- */
#if (USE_UTILS_DEBUG)
  #define DB_UART_INSTANCE      (USART1)
  #define DB_UART_GPIOPORT      (GPIOA)
  #define DB_UART_RX_PIN        (GPIO_PIN_10)
  #define DB_UART_TX_PIN        (GPIO_PIN_9)
  #define DB_UART_BAUDRATE      (115200UL)
#endif
/* ------------------------------------------ */


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
// 4. IAP_INFO_STRUCT_START_ADDR / IAP_INFO_STRUCT_REGION_SIZE:   Persistent structure storing update status,
//                                                                version, magic word, etc.
//       It is strongly recommended to place this structure in the last page or the last sector of the flash,
//       so that it is not accidentally overwritten by application or download region expansion.
//
// 5. IAP_MAGIC_WORD:   A known pattern written at the beginning of the IAP info structure to indicate
//                      valid update information (e.g., 0xAA55).
//
// Example for STM32F103ZET6 with 512KB total flash:
//   Bootloader:   0x08000000, 32KB
//   App:          0x08008000, 224KB
//   Download:     0x08040000, 224KB
//   IAP Info:     0x0807F800,  2KB  (last 2KB of 512KB)
//   Total: 32+224+224+2 = 482KB (fit in 512KB, leaving some free space)
/* -------------- Core Config ------------------- */
  #define BOOTLOADER_START_ADDRESS          (0x08000000UL)
  #define BOOTLOADER_SIZE                   (0x00008000UL)    // 32KB

  #define APP_START_ADDRESS                 (0x08008000UL)
  #define APP_REGION_SIZE                   (0x00038000UL)    // 224KB

  #define DOWNLOAD_START_ADDRESS            (0x08040000UL)
  #define DOWNLOAD_REGION_SIZE              (0x00038000UL)    // 224KB
  /* 32 + 224 + 224 = 480 KB. */

  #define IAP_INFO_STRUCT_START_ADDR        (0x0807F800UL)
  #define IAP_INFO_STRUCT_REGION_SIZE       (0x00000800UL)    // 2KB

  #define IAP_MAGIC_WORD                    (0xAA55UL)  
  #define SIZE_PER_PAGE_KB                  (2)              // 2KB Per Page.
/* ------------------------------------------ */


#endif 
