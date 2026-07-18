#include "main.h"


/* ---------------------------------------------- */
uint8_t wBuf[BYTES_PER_PACKS];
extern volatile BL_State_t g_bootloaderState;
static bool is_Retry = false;

#if (USE_POWER_FAIL_RESUME)
  extern BootResume_Data_t LoadConf;
#endif // USE_POWER_FAIL_RESUME
/* ---------------------------------------------- */


int main( void )
{
	Cus_Bootloader_Init();

	printf("Project Test OK!\nsystemCoreClock = :%u\n", SystemCoreClock);

	printf(" ============== Bootloader Running ================= \n");

	g_bootloaderState = BL_STATE_START;

	HAL_Delay(500);

	#if (USE_RECOVERY_APP)
		uint32_t bootCount = Cus_Bootloader_GetBootCount();
		if ( bootCount >= MAX_FAILED_COUNT )
		{
			// 连续重启超过阈值，APP 反复崩溃，且DOWNLOAD区可能已损坏.放弃主流程，直接进恢复区.
			Cus_Bootloader_ClearBootCount();
			Cus_Bootloader_JumpToRecoveryAPP();
			for( ; ; );			// 兜底. 程序不应该执行到这里.
		}

		Cus_Bootloader_IncreaseBootCount();		// 本次启动计数 + 1.(请在APP成功跳转后进行清除)
	#endif // USE_RECOVERY_APP

	uint8_t uReturn = Cus_Bootloader_CheckIAPRequest();
	if ( !uReturn )		
	{
		// No need to update. Jump to the APP.
		g_bootloaderState = BL_STATE_JUMP_APP;
	} 
	else 
	{
		g_bootloaderState = BL_STATE_ERASE_APP;
	}

	#if (USE_POWER_FAIL_RESUME)
		bool isLoad = g_BootResume->Load(&LoadConf);
		if ( isLoad )
		{
			// 状态更新 + 跳转.
			g_bootloaderState = LoadConf.state;
		}
	#endif // USE_POWER_FAIL_RESUME

	// 固件更新信息.
	IAP_Info_t iap_info;
    g_BootFlash->ReadIAP(IAP_INFO_STRUCT_START_ADDR, (uint8_t *)&iap_info, sizeof(iap_info));
	uint32_t writeSize = iap_info.app_size;
	uint32_t total_packs = (writeSize / BYTES_PER_PACKS);
	uint32_t remaining = (writeSize % BYTES_PER_PACKS);

	while(1)
	{
		Cus_Bootloader_FeedIWDG();

		switch (g_bootloaderState)
		{
			case BL_STATE_ERASE_APP:
			{
				#if (USE_POWER_FAIL_RESUME)
					LoadConf.state = BL_STATE_ERASE_APP;			// 记录 BL_STATE_ERASE_APP 状态.
					bool ldReturn = g_BootResume->Save(&LoadConf);
					if ( !ldReturn )
					{
						/* Err. TODO, */
					}
				#endif 

				int hReturn = g_BootFlash->Erase(APP_START_ADDRESS, APP_REGION_SIZE);
				if ( hReturn < 0 )
				{
					Cus_BootloaderHook_EraseFailed(APP_START_ADDRESS, hReturn);
					for( ; ; );
				}
				g_bootloaderState = BL_STATE_WRITE_FW;

				#if (USE_POWER_FAIL_RESUME)
					LoadConf.state = BL_STATE_WRITE_FW;
					ldReturn = g_BootResume->Save(&LoadConf);		// 记录 BL_STATE_WRITE_FW 状态.
					if ( !ldReturn )
					{
						/* TODO. */
					}
				#endif 

				break;
			}

			case BL_STATE_WRITE_FW:
			{
				static uint16_t current_packs = 0;
				static uint32_t current_downloadAddr = DOWNLOAD_START_ADDRESS;
				static uint32_t current_appAddr = APP_START_ADDRESS;

				#if (USE_POWER_FAIL_RESUME)
					static uint8_t resume_initialize = 0;
					if ( !resume_initialize )
					{
						/* Get the stored status parameters. */
						current_packs = LoadConf.packs;
						current_downloadAddr = (DOWNLOAD_START_ADDRESS + (LoadConf.packs * BYTES_PER_PACKS));
						current_appAddr = (APP_START_ADDRESS + (LoadConf.packs * BYTES_PER_PACKS));

						/* Locate the target byte address based on the recorded pack parameters. */
						uint32_t resume_offset = 0;
						volatile uint8_t *app = (volatile uint8_t *)current_appAddr;
						volatile uint8_t *download = (volatile uint8_t *)current_downloadAddr;

						for( uint32_t off = 0; off < BYTES_PER_PACKS; off++ )
						{
							if ( app[off] != download[off] )
							{
								/* Locate the byte boundary for the pending write prior to power loss. */
								resume_offset = off + 1;
								break;
							}
						}

						if ( resume_offset )
						{
							/* Resume: write only the remaining bytes of the current pack */
							uint32_t thisPackRemain = BYTES_PER_PACKS - resume_offset;
							memcpy(wBuf, (uint8_t *)current_downloadAddr, thisPackRemain);
							int hReturn = g_BootFlash->Write((current_appAddr + resume_offset), wBuf, thisPackRemain);
							if ( hReturn < 0 )	
							{
								/* TODO. */
							}

							/* Update. */
							current_packs++;
							current_appAddr 	 = APP_START_ADDRESS      + (current_packs * BYTES_PER_PACKS);
							current_downloadAddr = DOWNLOAD_START_ADDRESS + (current_packs * BYTES_PER_PACKS);

							if ( (current_packs == total_packs) && !remaining )
							{
								/* Firmware write complete. Advance to the next state. */
								g_bootloaderState = BL_STATE_VERIFY_FW;
								#if (USE_POWER_FAIL_RESUME)
									LoadConf.state = BL_STATE_VERIFY_FW;
									g_BootResume->Save(&LoadConf);
								#endif 
								continue;
							}
							else if ( (current_packs == total_packs) && remaining )
							{
								goto FLAG1;
							}
						}

						printf("\nPacks: %d, downloadAddr: %x, appAddr: %x\n", current_packs, current_downloadAddr, current_appAddr);
						resume_initialize = 1;		// 不可重入标志.
					}
				#endif 

				if ( is_Retry )
				{
					current_packs = 0;
					current_downloadAddr = DOWNLOAD_START_ADDRESS;
					current_appAddr = APP_START_ADDRESS;
				}

				memcpy(wBuf, (uint8_t *)current_downloadAddr, BYTES_PER_PACKS);
				Cus_Bootloader_FeedIWDG();

				int hReturn = g_BootFlash->Write(current_appAddr, wBuf, BYTES_PER_PACKS);
				Cus_Bootloader_FeedIWDG();
				if ( hReturn < 0 )
				{
					/* TODO. */
					Cus_BootloaderHook_WriteFailed(current_appAddr, hReturn);
				}

				#if (USE_POWER_FAIL_RESUME)
					/* Pack written successfully. Update the record. */
					LoadConf.packs++;
					g_BootResume->Save(&LoadConf);
				#endif 

				/* ---------- 测试 ------------------- */
				#if (RELEASE) == 0
					static uint8_t test = 0;
					test++;
					if ( (test == 3) && (*(volatile uint16_t *)(0x40006C28)) != 1 )
					{
						*(volatile uint16_t *)(0x40006C28) = 1;
						printf("\nPOWER_FIAL_RESUME_START_IN_5_SEC.\n");
						HAL_Delay(5000);
						NVIC_SystemReset();
					}
				#endif 
			/* ---------------------------------- */

				current_packs++;	
				current_downloadAddr += BYTES_PER_PACKS;						// 偏移到下个待读取的页.
				current_appAddr 	 += BYTES_PER_PACKS;					    // 偏移到下一个待写入的页.

FLAG1:
				if ( current_packs == total_packs )
				{	
					if ( remaining != 0 && remaining < BYTES_PER_PACKS )
					{
						// 剩余数据不足一页.
						Cus_Bootloader_FeedIWDG();
						memset(wBuf, 0, BYTES_PER_PACKS);
						memcpy(wBuf, (uint8_t *)current_downloadAddr, remaining);
						hReturn = g_BootFlash->Write(current_appAddr, wBuf, remaining);
						Cus_Bootloader_FeedIWDG();
						if ( hReturn < 0 )
						{
							Cus_BootloaderHook_WriteFailed(current_appAddr, hReturn);
						}
					}

					/* DOWNLOAD area fully written to APP. Update the state machine. */
					g_bootloaderState = BL_STATE_VERIFY_FW;

					#if (USE_POWER_FAIL_RESUME)
						/* Save PowerResume Conf. */
						LoadConf.state = BL_STATE_VERIFY_FW;
						g_BootResume->Save(&LoadConf);
					#endif 

					break;	
				}
				continue;
			}

		case BL_STATE_VERIFY_FW:
			{
				static uint8_t retry_count = 0;
				bool is_FW_VerifyOK = g_BootFlash->Verify(APP_START_ADDRESS, (uint8_t *)DOWNLOAD_START_ADDRESS, writeSize);
				Cus_Bootloader_FeedIWDG();
				if ( !is_FW_VerifyOK )
				{
					// Frameware Verify Failed. Start Retry.
					retry_count++;
					if ( retry_count <= 3 )
					{
						is_Retry = true;

						#if (USE_POWER_FAIL_RESUME)
							/* Clear the previous counter before retry to keep the power-loss state consistent with the Bootloader. */
							g_BootResume->Clear();
						#endif 

						g_bootloaderState = BL_STATE_ERASE_APP;		// Back to BL_STATE_ERASE_APP.
						break;
					}
					else 
					{
						// 重试 3 次全部失败，触发 VerifyFailed Hook（默认软复位）.
						Cus_BootloaderHook_VerifyFailed(APP_START_ADDRESS, writeSize);
					}
				}
				
				is_Retry = false;
				retry_count = 0;
				g_bootloaderState = BL_STATE_CLEAR_IAP_FLAG;

				#if (USE_POWER_FAIL_RESUME)
					LoadConf.state = BL_STATE_CLEAR_IAP_FLAG;
					g_BootResume->Save(&LoadConf);
				#endif 

				break;
			}

		case BL_STATE_CLEAR_IAP_FLAG:
			{
				Cus_Bootloader_FeedIWDG();
				
				int eReturn = g_BootFlash->Erase(IAP_INFO_STRUCT_START_ADDR, sizeof(IAP_Info_t));
				Cus_Bootloader_FeedIWDG();
				if ( eReturn < 0 )
				{
					Cus_BootloaderHook_EraseFailed(IAP_INFO_STRUCT_START_ADDR, eReturn);
					for( ; ; );
				}

				g_bootloaderState = BL_STATE_JUMP_APP;

				#if (USE_POWER_FAIL_RESUME)
					LoadConf.state = BL_STATE_JUMP_APP;
					g_BootResume->Save(&LoadConf);
				#endif 				

				break;
			}

		case BL_STATE_JUMP_APP:
			{
				// Jump to APP.
				Cus_Bootloader_FeedIWDG();

				uint32_t msp = *(volatile uint32_t *)APP_START_ADDRESS;		// 读取栈顶地址.
				if ( msp < MCU_SRAM_BASE_ADDR || msp > (MCU_SRAM_BASE_ADDR + MCU_SRAM_SIZE) )
				{
					// 栈顶地址非法.
					#if (USE_RECOVERY_APP)
						Cus_Bootloader_JumpToRecoveryAPP();	// 栈顶非法. 由于擦写和校验已成功，此处却栈顶出现问题. 极大可能DOWNLOAD区与APP区已经损毁. 开启USE_RECOVERY_APP功能的情况下直接跳转.
					#else 
						// 不启用恢复区，走通用错误 Hook（默认软复位）.
						Cus_BootloaderHook_GenericError(BL_STATE_JUMP_APP, IAP_ERRCODE_INVALID_STACKTOPADDR);
					#endif // USE_RECOVERY_APP

					for( ; ; );
				}

				uint32_t reset_vector = *(volatile uint32_t *)(APP_START_ADDRESS + 4);
				if ( reset_vector < APP_START_ADDRESS || reset_vector > (APP_START_ADDRESS + APP_REGION_SIZE) )
				{
					// 复位向量地址非法.
					#if (USE_RECOVERY_APP)
						Cus_Bootloader_JumpToRecoveryAPP();
					#else 
						Cus_BootloaderHook_GenericError(BL_STATE_JUMP_APP, IAP_ERRCODE_INVALID_RESETHANDLER);
					#endif // USE_RECOVERY_APP

					for( ; ; );
				}

				#if (USE_POWER_FAIL_RESUME)
					/* Clear the resume flag before the final jump to APP. */
					g_BootResume->Clear();		
				#endif 

				SysTick->CTRL = 0;          // 跳转前彻底关闭 SysTick，防止中断残留.
				SysTick->LOAD = 0;
				SysTick->VAL  = 0;
				
				SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;		// 清除可能已经挂起的 SysTick 中断请求.

				__disable_irq();
				HAL_DeInit();
				SCB->VTOR = APP_START_ADDRESS;		// 设置中断向量表.
				__DSB();
				__set_MSP(msp);							// 设置主堆栈.

				void (*app_entry)(void) = (void (*)(void))reset_vector;

				app_entry();

				for( ; ; );			// 正常情况下. 程序不应该运行到这里.
				break;
			}
		
		default:	break;
		}
	}
}

