#ifndef __BOOTLOADER_CONF_H__
#define __BOOTLOADER_CONF_H__


/* ****************************** */
  #include "stm32f1xx_hal.h"            // 按需更换所使用头文件.
/* ****************************** */


/* -------------- Feature ------------------- */
  #define RELEASE                 (1)

  #define USE_UTILS_DEBUG         (1)
  #define USE_UTILS_SYSCONF       (1)
  #define USE_RECOVERY_APP        (0)
  #define USE_POWER_FAIL_RESUME   (0)         // 是否启用断电续传. 0=不启用,升级过程中断电后下次启动重新进行擦写流程. 1=启用,升级过程断电后，下次启动继续烧写.
  #define USE_IWDG                (0)         // 接管IWDG喂狗操作. 0=未开启IWDG，耗时操作不处理喂狗. 1=开启IWDG，耗时操作自动处理喂狗. 
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


  #if (USE_RECOVERY_APP)
    #define RECOVERY_APP_START_ADDR         (0x0807C000UL)
    #define RECOVERY_APP_REGION_SIZE        (0x00004000UL)   // 16kb
    #define MAX_FAILED_COUNT                (4)              // 连续重启次数阈值

    #define RECOVERY_BKP_ADDR               (BKP_BASE)
    #define RECOVERY_BKP_MAGIC              (0xDEADUL)   // 验证魔数
  #endif // USE_RECOVERY_APP

/* ------------------------------------------ */



/* -------------- Error Define ------------------- */
  #define IAP_ERRCODE_INVALID_STACKTOPADDR  (0x01UL)
  #define IAP_ERRCODE_INVALID_RESETHANDLER  (0x01UL << 1)



/* ------------------------------------------ */

#endif 
