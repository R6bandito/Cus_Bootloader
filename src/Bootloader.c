#include "Bootloader.h"
#include <stdio.h>
#include <stdarg.h>


/* ==================== USER PORTING AREA ====================
 *
 * Put your porting template headers HERE.
 * The porting templates declare their own Install functions, e.g.:
 *
 *   #include "BootPlatform_stm32f1_Template.h"   // platform: clock / UART / watchdog
 *   #include "BootFlashPort_Template_kv.h"        // flash backend: erase / write / IAP request
 *
 * Then call the Install functions inside
 * Cus_Bootloader_InstallFunctions() below.
 *
 * =========================================================== */


/* ------------------- g_ver --------------------- */

volatile BL_State_t g_bootloaderState;

/* ---------------------------------------------- */


/* ---------------------------------------------- */
uint8_t Cus_Bootloader_CheckIAPRequest( void );
void Cus_Bootloader_Init( void );
uint32_t Cus_Bootloader_CRC32Caculate( uint8_t *pData, uint32_t data_len );
/* ---------------------------------------------- */


/* ********************************************* */
/*  USER INSTALLATION POINT                       */
/* ********************************************* */
/*
 * Cus_Bootloader_InstallFunctions()
 *
 * Central installation point of the Bootloader runtime.
 * The user MUST put ALL environment initializations inside this function:
 *   - Device / platform environment (clock, UART debug, watchdog, delay, ...)
 *   - Flash operation backend       (BootFlashPort: erase / write / read IAP request)
 *
 * This function is invoked by Cus_Bootloader_Init() at the very beginning
 * of the startup sequence, BEFORE any Bootloader service is used.
 * Implement your installations directly in the function body below.
 */
void 
Cus_Bootloader_InstallFunctions( void )
{
    /*
        * TODO: USER MUST IMPLEMENT.
        *
        * Put ALL environment / backend installations here.
        * Include your porting template headers in the USER PORTING AREA
        * at the top of this file, then call the Install functions, e.g.:
        *
        *   BootPlatform_stm32f1_Install();   // platform: clock / UART / watchdog
        *   BootFlashPort_kv_Install();       // flash backend: erase / write / IAP request
        */
}


void 
Cus_Bootloader_Init( void )
{
	/* User installation point: device environment & flash backend MUST be registered here. */
	Cus_Bootloader_InstallFunctions();

	/* Mandatory ports: the core calls these unconditionally. */
	BL_ASSERT( g_Platform->DelayMs && g_Platform->Init );
	BL_ASSERT( g_BootFlash->ReadIAP && g_BootFlash->ClearIAP );

	/* A/B: the slot-flag backend is called unconditionally
	   (startup slot resolution / VERIFY_AB) -- must be registered. */
	BL_ASSERT( g_BootFlash->ReadSlot && g_BootFlash->FlipSlot );

	/* Initialize the Bootloader runtime environment using the user-registered callbacks. */
	g_Platform->Init();

	if ( g_BootFlash->Init )
	{
		g_BootFlash->Init();
	}

	BL_Log( "[INFO] BOOT INIT PASS.\n", 0 );
}


uint8_t 
Cus_Bootloader_CheckIAPRequest( void )
{
	uint8_t buf[sizeof(IAP_Info_t)] = { 0 };
	bool isRead = g_BootFlash->ReadIAP( buf, sizeof(buf) );
	if ( !isRead )
	{
		BL_Log( "[INFO] NO IAP REQUEST.\n", 0 );
		return 0;
	}

	IAP_Info_t *iap_info = (IAP_Info_t *)buf;

	if ( iap_info->magic_word != IAP_MAGIC_WORD )   
	{
		BL_Log( "[WARN] DETECT IAP REQ. MAGIC DISMATCH. NO UPDATE EXECUTED.\n", 0 );
		return 0;     
	}

	if ( iap_info->app_size > APP_REGION_SIZE || iap_info->app_size == 0 )  
	{
		BL_LogF( 0, "[WARN] FIRMWARE SIZE %u INVALID (APP %u).\n",
				 iap_info->app_size, APP_REGION_SIZE );
		return 0;
	}

	/* A/B: the request CRC is verified against the TARGET slot in
	   BL_STATE_VERIFY_AB; nothing to check here. */
	BL_Log( "[INFO] IAP REQUEST GET. READY TO LOAD.\n", 0 );
	return 1;
}


uint32_t 
Cus_Bootloader_CRC32Caculate( uint8_t *pData, uint32_t data_len )
{
	uint32_t crc = 0xFFFFFFFF;    // CRC 初始值.
	const uint32_t *table = crc32_table;

	for( uint32_t i = 0; i < data_len; i++ )
	{
		crc = (crc >> 8) ^ table[(crc ^ pData[i]) & 0xFF];

		if ( i % 1024 == 0 )
		{
			BL_FeedDog();
		}
	}

	return crc ^ 0xFFFFFFFF; 
}


/* ****************************** Options: DFU ******************************************* */
  #if (USE_DFU_APP)

	/* Jump into the DFU APP (user's minimal transport layer) for firmware
	   download. Kept separate from the main jump logic on purpose.
	   @return true when the DFU APP was entered (the function never
	   returns on success); false when the DFU APP is not programmed or
	   corrupt -- the caller then falls back to the normal flow. */
	bool 
	Cus_Bootloader_JumpToDFUAPP( BL_ErrCode_t *eCode )
	{
		uint32_t dfu_msp = *(volatile uint32_t *)DFU_APP_START_ADDR;    /* Read the stack top. */
		if ( dfu_msp < MCU_SRAM_BASE_ADDR || dfu_msp > MCU_SRAM_BASE_ADDR + MCU_SRAM_SIZE )
		{
			/* DFU APP not programmed or corrupt: report and fall back,
			   the caller decides what to run next. */
			BL_Log( "[WARN] DFU APP INVALID (STACK TOP). FALL BACK.\n", 1 );
			return false;
		}

		uint32_t dfu_reset_vector = *(volatile uint32_t *)(DFU_APP_START_ADDR + 0x04UL);    /* Read the reset vector. */
		if ( dfu_reset_vector < DFU_APP_START_ADDR || dfu_reset_vector > DFU_APP_START_ADDR + DFU_APP_REGION_SIZE )
		{
			BL_Log( "[WARN] DFU APP INVALID (RESET VECTOR). FALL BACK.\n", 1 );
			return false;
		}

		/* Platform pre-jump hook: shut down SysTick, clear pending
		   interrupts, de-init peripherals, etc. Runs with interrupts
		   still enabled so the implementation may use HAL_Delay. */
		if ( g_Platform->PrepareJump )
		{
			g_Platform->PrepareJump();
		}

		__disable_irq();
		SCB->VTOR = DFU_APP_START_ADDR;
		__DSB();
		__set_MSP(dfu_msp);

		void (*reset_entry)(void) = (void (*)(void))dfu_reset_vector;
		reset_entry();

		/* The program should never reach here. */
		BL_ASSERT( 0 );

		/* Never reached; satisfies the bool return type. */
		return true;  
	}

  #endif // USE_DFU_APP
/* ******************************************************************************************** */


/* ***************************************************************************************** */

void BL_FeedDog( void )
{
    #if (USE_DG)
        if ( g_Platform->FeedDg ) 
		{
            g_Platform->FeedDg();
        }
    #endif
}


void BL_DelayMs( uint32_t ms )
{
    if ( g_Platform->DelayMs )
    {
        g_Platform->DelayMs(ms);
    }
}


void BL_Log( const char *msg, uint32_t err_code )
{
    #if ( USE_DEBUG )
        if ( g_Platform->LogOut ) 
		{
            g_Platform->LogOut(msg, err_code);
        }
    #else
        (void)msg;
        (void)err_code;
    #endif
}


void BL_LogF( uint32_t err_code, const char *fmt, ... )
{
    #if (USE_DEBUG)
        if ( g_Platform->LogOut ) 
		{
            static char buf[256];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);
            g_Platform->LogOut(buf, err_code);
        }
    #else
        (void)err_code;
        (void)fmt;
    #endif
}

/* ***************************************************************************************** */


/* ****************************** Hook Default ******************************************* */
__weak void Cus_BootloaderHook_EraseFailed( uint32_t page_addr, int error )
{
	(void)(page_addr);
	(void)(error);

	#if (USE_UTILS_DEBUG)
		printf("Cus_BootloaderHook_FailedToErase Trigged!\n\n");
		printf("[BOOT] Erase Failed! Addr:0x%08X, Err:%d. System will reset in 3s...\n", page_addr, error);
	#endif
}


__weak void Cus_BootloaderHook_WriteFailed( uint32_t target_addr, int error )
{
	(void)(target_addr);
	(void)(error);

	#if (USE_UTILS_DEBUG)
		printf("Cus_BootloaderHook_WriteFWFailed Trigged!\n\n");
		printf("[BOOT] Write Failed! Addr:0x%08X, Err:%d. System will reset in 3s...\n", target_addr, error);
	#endif  
}


__weak void Cus_BootloaderHook_VerifyFailed( uint32_t region_start, uint32_t size )
{
	(void)(region_start);
	(void)(size);

	#if (USE_UTILS_DEBUG)
		printf("Cus_BootloaderHook_VerifyFailed Trigged!\n\n");
		printf("[BOOT] Verify Failed! Region:0x%08X, Size:%u. System will reset in 3s...\n", region_start, size);
	#endif  
}


__weak void Cus_BootloaderHook_GenericError( BL_State_t state, uint32_t error_code )
{
	(void)(state);
	(void)(error_code);

	#if (USE_UTILS_DEBUG)
		printf("Cus_BootloaderHook_GenericError Trigged!\n\n");
		printf("[BOOT] Generic Error! State:%d, Code:0x%02X. System will reset in 3s...\n", state, error_code);
	#endif  
}


__weak void Cus_BootloaderHook_DFUEnterFailed( BL_State_t state, uint32_t errcode )
{
	(void)(state);
	(void)(errcode);

	#if (USE_UTILS_DEBUG)
		printf("Cus_BootloaderHook_DFUEnterFailed Trigged!\n\n");
		printf("[BOOT] DFU APP Unavailable! State:%d, Code:0x%02X. System halted.\n", state, errcode);
	#endif  
}
/* ****************************** Hook Default ******************************************* */

