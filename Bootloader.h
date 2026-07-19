#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__


#include "BootloaderConf.h"
#include "BootFlashPort.h"
#include "Bootloader_Utils.h"
#include "crc32table.h"
#include "IAP_Protocol.h"

#if (USE_POWER_FAIL_RESUME)
	#include "BootResume.h"
#endif /* USE_POWER_FAIL_RESUME */


typedef enum 
{
	BL_STATE_START = 0,
	BL_STATE_CHECK_FLAG,
	BL_STATE_VERIFY_CRC,
	BL_STATE_ERASE_APP,
	BL_STATE_WRITE_FW,
	BL_STATE_VERIFY_FW,
	BL_STATE_CLEAR_IAP_FLAG,
	BL_STATE_JUMP_APP,

} BL_State_t;


/* ---------------------------------------------------------- */
uint8_t Cus_Bootloader_CheckIAPRequest( void );
void Cus_Bootloader_Init( void );
void Cus_Bootloader_FeedIWDG( void );


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
