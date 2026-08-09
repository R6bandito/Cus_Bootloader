#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__


#include "BootloaderConf.h"
#include "BootFlashPort.h"
#include "BootPlatform.h"
#include "crc32table.h"
#include "IAP_Protocol.h"


#if (USE_POWER_FAIL_RESUME)
	#include "BootResume.h"
#endif /* USE_POWER_FAIL_RESUME */


typedef enum 
{
	BL_STATE_START = 0,
	BL_STATE_ERASE_APP,
	BL_STATE_WRITE_FW,
	BL_STATE_VERIFY_FW,
	BL_STATE_VERIFY_AB,
	BL_STATE_CLEAR_IAP_FLAG,
	BL_STATE_JUMP_APP,

} BL_State_t;


/* ---------------------------------------------------------- */
uint8_t Cus_Bootloader_CheckIAPRequest( void );
void Cus_Bootloader_Init( void );
uint32_t Cus_Bootloader_CRC32Caculate( uint8_t *pData, uint32_t data_len );
void Cus_Bootloader_InstallFunctions( void );


/* ---------------------------------------------------------- */
void BL_FeedDog( void );
void BL_DelayMs( uint32_t ms );
void BL_Log( const char *msg, uint32_t err_code );
void BL_LogF( uint32_t err_code, const char *fmt, ... );


/* ---------------------------------------------------------- */
/*  BL_ASSERT(cond)                                            */
/*  Runtime assertion for the Bootloader.                      */
/*  When the condition fails:                                  */
/*    - one log line is emitted BEFORE the hang (file and      */
/*      condition in the message, line number as err_code),    */
/*      only when USE_DEBUG is enabled and g_Platform->LogOut  */
/*      is registered (BL_Log already performs both checks).   */
/*    - the device then hangs in an infinite loop, feeding the */
/*      watchdog every BL_ASSERT_FEED_INTERVAL_MS via          */
/*      BL_FeedDog() / BL_DelayMs() when USE_DG is enabled     */
/*      (both are no-ops otherwise).                           */
/* ---------------------------------------------------------- */
#define BL_ASSERT_FEED_INTERVAL_MS   (50UL)   /* must be far below the IWDG timeout */
#define BL_ASSERT(cond) \
    do { \
        if ( !(cond) ) { \
            BL_Log( "BL_ASSERT failed: " __FILE__ ": " #cond, __LINE__ ); \
            while (1) { \
                BL_FeedDog(); \
                BL_DelayMs( BL_ASSERT_FEED_INTERVAL_MS ); \
            } \
        } \
    } while (0)


/* ---------------------- Options: Revocery -------------------------- */
  #if (USE_RECOVERY_APP)
    void Cus_Bootloader_RecoveryInit( void );
    uint32_t Cus_Bootloader_GetBootCount( void );
    void Cus_Bootloader_IncreaseBootCount( void );
    void Cus_Bootloader_ClearBootCount( void );
    void Cus_Bootloader_JumpToRecoveryAPP( void );
  #endif // USE_RECOVERY_APP
/* -------------------------------------------------------------------- */


// 擦除失败时的 Hook（page_addr：失败的页地址，error：错误码）
__weak void Cus_BootloaderHook_EraseFailed( uint32_t page_addr, int error );

// 写入失败时的 Hook（target_addr：写入的目标地址，error：错误码）
__weak void Cus_BootloaderHook_WriteFailed(uint32_t target_addr, int error);

// 固件验证失败时的 Hook（region_start：验证起始地址，size：验证大小）
__weak void Cus_BootloaderHook_VerifyFailed(uint32_t region_start, uint32_t size);

// 通用错误 Hook（state：当前状态，error_code：自定义错误码）
__weak void Cus_BootloaderHook_GenericError(BL_State_t state, uint32_t error_code);
/* ---------------------------------------------------------- */


#endif // __BOOTLOADER_H__
