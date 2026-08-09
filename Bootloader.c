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
 *   #include "BootResume_Template_kv.h"           // power-fail resume (optional, if USE_POWER_FAIL_RESUME)
 *
 * Then call the Install functions inside
 * Cus_Bootloader_InstallFunctions() below.
 *
 * =========================================================== */


/* ------------------- g_ver --------------------- */

volatile BL_State_t g_bootloaderState;

#if (USE_POWER_FAIL_RESUME)
	BootResume_Data_t LoadConf;
#endif // USE_POWER_FAIL_RESUME

/* ---------------------------------------------- */


/* ---------------------------------------------- */
uint8_t Cus_Bootloader_CheckIAPRequest( void );
void Cus_Bootloader_Init( void );

static uint8_t Cus_Bootloader_CRC32Verify( uint32_t exptected_CRC );
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
 *   - Optional power-fail resume backend (BootResume, if enabled)
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
        *   BootResume_kv_Install();          // power-fail resume (optional)
        */
}


void 
Cus_Bootloader_Init( void )
{
	/* User installation point: device environment & flash backend MUST be registered here. */
	Cus_Bootloader_InstallFunctions();

	/* These port is necessary. */
	BL_ASSERT( g_Platform->DelayMs && g_Platform->Init );
	BL_ASSERT( g_BootFlash->ReadIAP && g_BootFlash->Verify && g_BootFlash->Write
					 && g_BootFlash->Erase && g_BootFlash->ClearIAP );

	#if (USE_POWER_FAIL_RESUME)
		BL_ASSERT( g_BootResume->Load && g_BootResume->Clear && g_BootResume->Save );
	#endif /* USE_POWER_FAIL_RESUME */

	/* Initialize the Bootloader runtime environment using the user-registered callbacks. */
	g_Platform->Init();

	if ( g_BootFlash->Init )
	{
		g_BootFlash->Init();
	}

	#if (USE_RECOVERY_APP)
		Cus_Bootloader_RecoveryInit();
	#endif // USE_RECOVERY_APP

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

	if ( iap_info->app_size > APP_REGION_SIZE || iap_info->app_size > DOWNLOAD_REGION_SIZE 
			|| iap_info->app_size == 0 )  
	{
		BL_LogF( 0, "[WARN] FIRMWARE SIZE %u INVALID (APP %u / DL %u).\n",
				 iap_info->app_size, APP_REGION_SIZE, DOWNLOAD_REGION_SIZE );
		return 0;
	}

	#if (!USE_AB_SLOT)
	uint8_t CRC_CheckReturn = Cus_Bootloader_CRC32Verify( iap_info->CRC32 );
	if ( !CRC_CheckReturn )   
	{
		/* CRC Verify Failed! Fireware not reliable.Discard this update required. */ 
		BL_Log( "[WARN] DETECT FIRMWARE BUT CRC VERIFY ERROR. SKIP.\n", 1 );
		int hReturn = g_BootFlash->ClearIAP();
		if ( hReturn < 0 )
			BL_Log( "[ERROR] IAP REQUEST CLEAR FAILED.\n", 1 );

		return 0;
	}
	#endif /* !USE_AB_SLOT */

	/* CRC Verify Success. */ 
	BL_Log( "[INFO] IAP REQUEST GET. READY TO LOAD.\n", 0 );
	return 1;
}


static uint8_t 
Cus_Bootloader_CRC32Verify( uint32_t exptected_CRC )
{
	uint8_t buf[sizeof(IAP_Info_t)] = { 0 };
	bool isRead = g_BootFlash->ReadIAP(buf, sizeof(buf));
	if ( !isRead )
	{
		return 0;
	}

	IAP_Info_t *iap_info = (IAP_Info_t *)buf;

	uint32_t CalculateCRC = Cus_Bootloader_CRC32Caculate((uint8_t *)DOWNLOAD_START_ADDRESS, iap_info->app_size);

	if ( CalculateCRC != exptected_CRC )  return 0;   // CRC Verify Failed!

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


/* ****************************** Options: Revocery ******************************************* */
  #if (USE_RECOVERY_APP)

	void Cus_Bootloader_RecoveryInit( void )
	{
		// 解除 BKP 寄存器写保护.
		uint32_t rcc_temp = RCC->APB1ENR;
		rcc_temp |= (0x01UL << 28);         // PWREN 置 1.
		rcc_temp |= (0x01UL << 27);         // BKPEN 置 1.
		RCC->APB1ENR = rcc_temp;

		uint32_t pwr_temp = PWR->CR;
		pwr_temp |= (0x01UL << 8);          // DBP 置 1.
		PWR->CR = pwr_temp;
	}


	uint32_t Cus_Bootloader_GetBootCount( void )
	{
		uint32_t bkp_1registerAddr = RECOVERY_BKP_ADDR;

		// 魔数不匹配 = 首次上电或掉电过，计数归零
		if ( *(volatile uint32_t *)bkp_1registerAddr != RECOVERY_BKP_MAGIC )  return 0;  // 魔数校验失败.返回 0 表示正常启动

		uint32_t bkp_2registerAddr = (RECOVERY_BKP_ADDR + 0x04UL);        // 第二个 BKP 寄存器.
		return *(volatile uint32_t *)bkp_2registerAddr;
	}


	void Cus_Bootloader_IncreaseBootCount( void )
	{
		uint32_t bkp_1registerAddr = RECOVERY_BKP_ADDR;
		if ( *(volatile uint32_t *)bkp_1registerAddr != RECOVERY_BKP_MAGIC )  
		{
			// 首次写入时，写入魔数验证码进行初始化.
			uint32_t *bkp_1registerpAddr = (volatile uint32_t *)(RECOVERY_BKP_ADDR + 0x00UL);  // 首个BKP寄存器.
			*bkp_1registerpAddr = RECOVERY_BKP_MAGIC;
		}

		uint32_t bkp_2registerAddr = (RECOVERY_BKP_ADDR + 0x04UL);
		*(volatile uint32_t *)bkp_2registerAddr += 1;
	}


	void Cus_Bootloader_ClearBootCount( void )
	{
		uint32_t bkp_1registerAddr = RECOVERY_BKP_ADDR;
		if ( *(volatile uint32_t *)bkp_1registerAddr != RECOVERY_BKP_MAGIC  )   return;

		*(volatile uint32_t *)bkp_1registerAddr = 0x00;   // 清除魔数.

		uint32_t bkp_2registerAddr = (RECOVERY_BKP_ADDR + 0x04UL);
		*(volatile uint32_t *)bkp_2registerAddr = 0x00;   // 清除数据.
	}


	void Cus_Bootloader_JumpToRecoveryAPP( void )     // 此处为便于独立逻辑，并未将其与main中的Jump逻辑进行整合合并为统一接口.
	{
		uint32_t recovery_msp = *(volatile uint32_t *)RECOVERY_APP_START_ADDR;    // 读取栈顶.
		if ( recovery_msp < MCU_SRAM_BASE_ADDR || recovery_msp > MCU_SRAM_BASE_ADDR + MCU_SRAM_SIZE )
		{
			// 最后恢复区APP栈顶地址非法.说明该区域从未被烧录或已损坏.
			// 此时三层固件（APP、DOWNLOAD、RECOVERY）全部失效.
			// 打印最后的诊断信息.
			#if (USE_UTILS_DEBUG)
				printf("========================================\n");
				printf("\n=== FATAL: ALL FIRMWARE CORRUPTED ===\n");
				printf("Recovery APP stack top: 0x%08X (invalid)\n", recovery_msp);
				printf("Device halted. Re-program required.\n");
				printf("========================================\n");
			#endif

			for( ; ; )    // 死循环兜底.
			{
				#if (USE_IWDG)
				HAL_Delay(5);   
				#endif // USE_IWDG
			}   
		}

		uint32_t recovery_reset_vector = *(volatile uint32_t *)(RECOVERY_APP_START_ADDR + 0x04UL);    // 取出复位向量.
		if ( recovery_reset_vector < RECOVERY_APP_START_ADDR || recovery_reset_vector > RECOVERY_APP_START_ADDR + RECOVERY_APP_REGION_SIZE )
		{
			#if (USE_UTILS_DEBUG)
				printf("========================================\n");
				printf(" \nFATAL ERROR: ALL FIRMWARE CORRUPTED\n");
				printf("Bootloader: OK (0x%08X)\n", BOOTLOADER_START_ADDRESS);
				printf("APP:        CORRUPTED\n");
				printf("Download:   CORRUPTED or EMPTY\n");
				printf("Recovery:   CORRUPTED or NOT PROGRAMMED\n");
				printf("Recovery reset vector: 0x%08X (invalid)\n", recovery_reset_vector);
				printf("\nDevice halted. Please re-program via debugger.\n");
				printf("========================================\n");
			#endif

			for( ; ; )    // 死循环兜底.
			{
				#if (USE_IWDG)
				HAL_Delay(5);   
				#endif // USE_IWDG
			}   
		}

		/* Platform pre-jump hook: shut down SysTick, clear pending
		   interrupts, de-init peripherals, etc. Runs with interrupts
		   still enabled so the implementation may use HAL_Delay. */
		if ( g_Platform->PrepareJump )
		{
			g_Platform->PrepareJump();
		}

		__disable_irq();
		SCB->VTOR = RECOVERY_APP_START_ADDR;
		__DSB();
		__set_MSP(recovery_msp);

		void (*reset_entry)(void) = (void (*)(void))recovery_reset_vector;
		reset_entry();

		for( ; ; );   // 程序不应该执行到这里.
	}

  #endif // USE_RECOVERY_APP
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
/* ****************************** Hook Default ******************************************* */

