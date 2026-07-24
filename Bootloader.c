#include "Bootloader.h"
#include <stdio.h>
#include "BootFlashPort_Template_kv.h"

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
static uint32_t Cus_Bootloader_CRC32Caculate( uint8_t *pData, uint32_t data_len );
/* ---------------------------------------------- */


	void Cus_Bootloader_Init( void )
	{
		HAL_Init();

		bootloader_InstallCallbacks();

		g_BootFlash->Init();

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

		#if (USE_RECOVERY_APP)
			Cus_Bootloader_RecoveryInit();
		#endif // USE_RECOVERY_APP

		Cus_Bootloader_FeedIWDG();
	}


	uint8_t Cus_Bootloader_CheckIAPRequest( void )
	{
		uint8_t buf[sizeof(IAP_Info_t)] = { 0 };
		bool isRead = g_BootFlash->ReadIAP(buf, sizeof(buf));
		if ( !isRead )
		{
			return 0;
		}

		IAP_Info_t *iap_info = (IAP_Info_t *)buf;

		g_bootloaderState = BL_STATE_CHECK_FLAG;
		if ( iap_info->magic_word != IAP_MAGIC_WORD )   
			return 0;     // No IAP Update Request.

		if ( iap_info->app_size > DOWNLOAD_REGION_SIZE || iap_info->app_size == 0 )  
			return 0;   // Size Error.

		g_bootloaderState = BL_STATE_VERIFY_CRC;
		uint8_t CRC_CheckReturn = Cus_Bootloader_CRC32Verify(iap_info->CRC32);
		if ( !CRC_CheckReturn )   
		{
			// CRC Verify Failed! Fireware not reliable.Discard this update required.
			int hReturn = g_BootFlash->ClearIAP();
			if ( hReturn < 0 )
			{
				/* TODO. */
			}
			return 0;
		}

		// CRC Verify Success.
		return 1;
	}


	static uint8_t Cus_Bootloader_CRC32Verify( uint32_t exptected_CRC )
	{
		uint8_t buf[sizeof(IAP_Info_t)] = { 0 };
		bool isRead = g_BootFlash->ReadIAP(buf, sizeof(buf));
		if ( !isRead )
		{
			return 0;
		}

		IAP_Info_t *iap_info = (IAP_Info_t *)buf;

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
		Cus_Bootloader_FeedIWDG();

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
				Cus_Bootloader_FeedIWDG();    // 喂狗防止复位(APP DOWNLOAD RESCUE APP区均已出错. 再复位已无意义).
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
				Cus_Bootloader_FeedIWDG();    // 喂狗防止复位(APP DOWNLOAD RESCUE APP区均已出错. 再复位已无意义).
				HAL_Delay(5);   
				#endif // USE_IWDG
			}   
		}

		SysTick->CTRL = 0;          // 跳转前彻底关闭 SysTick，防止中断残留.
		SysTick->LOAD = 0;
		SysTick->VAL  = 0;
		SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;		// 清除可能已经挂起的 SysTick 中断请求.

		__disable_irq();
		HAL_DeInit();
		SCB->VTOR = RECOVERY_APP_START_ADDR;
		__DSB();
		__set_MSP(recovery_msp);

		Cus_Bootloader_FeedIWDG();    // 跳转前最后一次喂狗.
		void (*reset_entry)(void) = (void (*)(void))recovery_reset_vector;
		reset_entry();

		for( ; ; );   // 程序不应该执行到这里.
	}

  #endif // USE_RECOVERY_APP
/* ******************************************************************************************** */


/* ****************************** Hook Default ******************************************* */

__weak void Cus_BootloaderHook_EraseFailed( uint32_t page_addr, int error )
{
  UNUSED(page_addr);
  UNUSED(error);

  #if (USE_UTILS_DEBUG)
    printf("Cus_BootloaderHook_FailedToErase Trigged!\n\n");
    printf("[BOOT] Erase Failed! Addr:0x%08X, Err:%d. System will reset in 3s...\n", page_addr, error);
  #endif

  HAL_Delay(500);
  NVIC_SystemReset();
}


__weak void Cus_BootloaderHook_WriteFailed( uint32_t target_addr, int error )
{
  UNUSED(target_addr);
  UNUSED(error);

  #if (USE_UTILS_DEBUG)
    printf("Cus_BootloaderHook_WriteFWFailed Trigged!\n\n");
    printf("[BOOT] Write Failed! Addr:0x%08X, Err:%d. System will reset in 3s...\n", target_addr, error);
  #endif  

  HAL_Delay(500);
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

  HAL_Delay(500);
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

  HAL_Delay(500);
  NVIC_SystemReset();
}

/* ***************************************************************************************** */

