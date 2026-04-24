#include "Bootloader.h"

/* ------------------- g_ver --------------------- */
volatile BL_State_t g_bootloaderState;
/* ---------------------------------------------- */

/* ---------------------------------------------- */
uint8_t Cus_Bootloader_CheckIAPRequest( void );
void Cus_Bootloader_Init( void );

static uint8_t Cus_Bootloader_CRC32Verify( uint32_t exptected_CRC );
static uint32_t Cus_Bootloader_CRC32Caculate( uint8_t *pData, uint32_t data_len );
/* ---------------------------------------------- */


void Cus_Bootloader_Init( void )
{
  HAL_Init();

  #if (USE_UTILS_SYSCONF)
    Cus_Bootloader_Utils_SystemClockConfig();
  #else
    #warning "User must provide a Custom SystemConfig API to Initialize the SystemClock if you dont use the default."
    {
      // Your API Called at Here.
      // e.g. Custom_SystemClockConfig();
    }
  #endif // USE_UTILS_SYSCONF
  
  #if (USE_UTILS_DEBUG)
    Cus_Bootloader_Utils_DebugUART();
  #endif // USE_UTILS_DEBUG

  Cus_Bootloader_FeedIWDG();

  Cus_Flash_CalibrateLatency();
}


uint8_t Cus_Bootloader_CheckIAPRequest( void )
{
  IAP_Info_t *iap_info = (IAP_Info_t *)IAP_INFO_STRUCT_START_ADDR;

  g_bootloaderState = BL_STATE_CHECK_FLAG;
  if ( iap_info->magic_word != IAP_MAGIC_WORD )   return 0;     // No IAP Update Request.

  if ( iap_info->app_size > DOWNLOAD_REGION_SIZE || iap_info->app_size == 0 )  return 0;   // Size Error.

  g_bootloaderState = BL_STATE_VERIFY_CRC;
  uint8_t CRC_CheckReturn = Cus_Bootloader_CRC32Verify(iap_info->CRC32);
  if ( !CRC_CheckReturn )   
  {
    // CRC Verify Failed! Fireware not reliable.Discard this update required.
    uint32_t IAP_Addr_Page = Cus_Flash_GetPageStart(IAP_INFO_STRUCT_START_ADDR);
    Cus_Flash_State_t hReturn = Cus_Flash_ErasePage(IAP_Addr_Page);
    if ( hReturn != CUS_FLASH_OK )
    {
      Cus_BootloaderHook_EraseFailed( IAP_INFO_STRUCT_START_ADDR, hReturn );
    }
    return 0;
  }

  // CRC Verify Success.
  return 1;
}


static uint8_t Cus_Bootloader_CRC32Verify( uint32_t exptected_CRC )
{
  IAP_Info_t *iap_info = (IAP_Info_t *)IAP_INFO_STRUCT_START_ADDR;

  uint32_t CalculateCRC = Cus_Bootloader_CRC32Caculate((uint8_t *)DOWNLOAD_START_ADDRESS, iap_info->app_size);

  Cus_Bootloader_FeedIWDG();

  if ( CalculateCRC != exptected_CRC )  return 0;   // CRC Verify Failed!

  return 1;   
}


static uint32_t Cus_Bootloader_CRC32Caculate( uint8_t *pData, uint32_t data_len )
{
  uint32_t crc = 0xFFFFFFFF;    // CRC 初始值.
  const uint32_t *table = crc32_table;

  for( uint32_t i = 0; i < data_len; i++ )
  {
    crc = (crc >> 8) ^ table[(crc ^ pData[i]) & 0xFF];

    if ( i % 1024 == 0 )
    {
      Cus_Bootloader_FeedIWDG();    // 每1024字节喂狗一次.
    }
  }

  return crc ^ 0xFFFFFFFF; 
}



void Cus_Bootloader_FeedIWDG( void )
{
  #if (USE_IWDG)
    uint16_t Reload = 0xAAAAUL;

    IWDG->KR = (Reload & 0xFFFFUL);
  #endif
  __nop();
}


/* ****************************** Hook Default ******************************************* */

__weak void Cus_BootloaderHook_EraseFailed( uint32_t page_addr, Cus_Flash_State_t error )
{
  UNUSED(page_addr);
  UNUSED(error);

  #if (USE_UTILS_DEBUG)
    printf("Cus_BootloaderHook_FailedToErase Trigged!\n\n");
    printf("[BOOT] Erase Failed! Addr:0x%08X, Err:%d. System will reset in 3s...\n", page_addr, error);
  #endif

  HAL_Delay(3000);
  NVIC_SystemReset();
}


__weak void Cus_BootloaderHook_WriteFailed( uint32_t target_addr, Cus_Flash_State_t error )
{
  UNUSED(target_addr);
  UNUSED(error);

  #if (USE_UTILS_DEBUG)
    printf("Cus_BootloaderHook_WriteFWFailed Trigged!\n\n");
    printf("[BOOT] Write Failed! Addr:0x%08X, Err:%d. System will reset in 3s...\n", target_addr, error);
  #endif  

  HAL_Delay(3000);
  NVIC_SystemReset();
}


__weak void Cus_BootloaderHook_VerifyFailed( uint32_t region_start, uint32_t size )
{
  UNUSED(region_start);
  UNUSED(size);

  #if (USE_UTILS_DEBUG)
    printf("Cus_BootloaderHook_VerifyFailed Trigged!\n\n");
    printf("[BOOT] Verify Failed! Region:0x%08X, Size:%u. System will reset in 3s...\n", region_start, size);
  #endif  

  HAL_Delay(3000);
  NVIC_SystemReset();
}


__weak void Cus_BootloaderHook_GenericError( BL_State_t state, uint32_t error_code )
{
  UNUSED(state);
  UNUSED(error_code);

  #if (USE_UTILS_DEBUG)
    printf("Cus_BootloaderHook_GenericError Trigged!\n\n");
    printf("[BOOT] Generic Error! State:%d, Code:0x%02X. System will reset in 3s...\n", state, error_code);
  #endif  

  HAL_Delay(3000);
  NVIC_SystemReset();
}

/* ***************************************************************************************** */

