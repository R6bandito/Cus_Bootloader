#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__


#include "BootloaderConf.h"
#include "BootFlashPort.h"
#include "BootPlatform.h"
#include "crc32table.h"
#include "IAP_Protocol.h"


typedef enum 
{
	BL_STATE_START = 0,
	BL_STATE_VERIFY_AB,
	BL_STATE_CLEAR_IAP_FLAG,
	BL_STATE_JUMP_APP,

} BL_State_t;


/* Bootloader error codes, passed to the hooks as error_code / errcode. */
typedef enum
{
	IAP_ERRCODE_INVALID_STACKTOPADDR = 0x01UL,
	IAP_ERRCODE_INVALID_RESETHANDLER = 0x02UL,
	
} BL_ErrCode_t;


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


/* ---------------------- Options: DFU -------------------------- */
  #if (USE_DFU_APP)
    /* Try to jump into the DFU APP. Returns false when the DFU APP is
       not programmed / corrupt -- the caller falls back to the normal
       flow. Never returns on success. */
    bool Cus_Bootloader_JumpToDFUAPP( BL_ErrCode_t *eCode );
  #endif // USE_DFU_APP
/* -------------------------------------------------------------------- */


// 擦除失败时的 Hook（page_addr：失败的页地址，error：错误码）
__weak void Cus_BootloaderHook_EraseFailed( uint32_t page_addr, int error );

// 写入失败时的 Hook（target_addr：写入的目标地址，error：错误码）
__weak void Cus_BootloaderHook_WriteFailed(uint32_t target_addr, int error);

// 固件验证失败时的 Hook（region_start：验证起始地址，size：验证大小）
__weak void Cus_BootloaderHook_VerifyFailed(uint32_t region_start, uint32_t size);

// 通用错误 Hook（state：当前状态，error_code：自定义错误码）
__weak void Cus_BootloaderHook_GenericError(BL_State_t state, uint32_t error_code);

/* Called when the DFU APP is unavailable (not programmed / corrupt)
   while the run slot is also invalid (stack top / reset vector check
   failed). Non-blocking: the core halts AFTER this hook returns.
   User may redefine for indicator / log / soft reset.
   @param state:  the BL_State_t that detected the failure.
   @param errcode: the BL_ErrCode_t that triggered the DFU attempt. */
__weak void Cus_BootloaderHook_DFUEnterFailed( BL_State_t state, uint32_t errcode );
/* ---------------------------------------------------------- */


#endif // __BOOTLOADER_H__
