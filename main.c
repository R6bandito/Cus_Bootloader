#include "main.h"

extern volatile BL_State_t g_bootloaderState;

Cus_Flash_Page_t *pPage;

static bool is_Retry = false;



int main( void )
{
	Cus_Bootloader_Init();

	printf("Project Test OK!\nsystemCoreClock = :%u\n", SystemCoreClock);

	printf(" ============== Bootloader Running ================= \n");

	g_bootloaderState = BL_STATE_START;

	uint8_t hReturn = Cus_Bootloader_CheckIAPRequest();
	if ( !hReturn )		
	{
		// No need to update. Jump to the APP.
		g_bootloaderState = BL_STATE_JUMP_APP;
		goto STATE_CHECK;
	} 
	else 
	{
		g_bootloaderState = BL_STATE_ERASE_APP;
	}

	// 固件更新信息.
	uint32_t writeSize = ((IAP_Info_t *)IAP_INFO_STRUCT_START_ADDR)->app_size;
	uint32_t total_pages = (writeSize / (SIZE_PER_PAGE_KB * 1024));
	uint32_t remaining = (writeSize % (SIZE_PER_PAGE_KB * 1024));

	Factory_GetPageControlBlock(&pPage);

	while(1)
	{
STATE_CHECK:
		Cus_Bootloader_FeedIWDG();

		switch (g_bootloaderState)
		{
			case BL_STATE_ERASE_APP:
			{
				uint32_t page_size = SIZE_PER_PAGE_KB * 1024;
				uint32_t erase_count = (APP_REGION_SIZE + page_size - 1) / page_size;		// 向上取整.防止因APP START地址问题导致漏擦除.
				uint16_t SuccessErasePages = Cus_Flash_ErasePages(APP_START_ADDRESS, erase_count);
				if ( SuccessErasePages != erase_count )
				{
					Cus_BootloaderHook_EraseFailed( (APP_START_ADDRESS + (SuccessErasePages * 1024)), CUS_FLASH_ERROR );
					for( ; ; );
				}
				g_bootloaderState = BL_STATE_WRITE_FW;

				break;
			}

			case BL_STATE_WRITE_FW:
			{
				static uint16_t current_pages = 0;
				static uint32_t current_downloadAddr = DOWNLOAD_START_ADDRESS;
				static uint32_t current_appAddr = APP_START_ADDRESS;

				if ( is_Retry )
				{
					current_pages = 0;
					current_downloadAddr = DOWNLOAD_START_ADDRESS;
					current_appAddr = APP_START_ADDRESS;
				}

				pPage->Reset(pPage);

				pPage->PageAddress = current_appAddr;
				memcpy(pPage->PageDataBuffer, (uint8_t *)current_downloadAddr, (SIZE_PER_PAGE_KB * 1024));
				Cus_Bootloader_FeedIWDG();

				Cus_Flash_State_t hReturn = Cus_Flash_WritePage(pPage);
				Cus_Bootloader_FeedIWDG();
				if ( hReturn != CUS_FLASH_OK )
				{
					Cus_BootloaderHook_WriteFailed(pPage->PageAddress, hReturn);
					for( ; ; );								// 写入失败. 由于已经擦除APP区. 此处失败后进入死循环. 待下一步处理.
				}

				current_pages++;	
				current_downloadAddr += (SIZE_PER_PAGE_KB * 1024);		// 偏移到下个待读取的页.
				current_appAddr += (SIZE_PER_PAGE_KB * 1024);					// 偏移到下一个待写入的页.

				if ( current_pages == total_pages )
				{	
					// 整数页已经写入完. 
					if ( remaining == 0 )
					{
						// 无剩余不到一个页大小的数据.
						g_bootloaderState = BL_STATE_VERIFY_FW;
						pPage->Release(pPage);
						break;	
					}

					if ( remaining != 0 && remaining < (SIZE_PER_PAGE_KB * 1024) )
					{
						// 剩余数据不足一页.
						Cus_Bootloader_FeedIWDG();
						memset(pPage->PageDataBuffer, 0, (SIZE_PER_PAGE_KB * 1024));
						memcpy(pPage->PageDataBuffer, (uint8_t *)current_downloadAddr, remaining);
						pPage->PageAddress = current_appAddr;

						Cus_Flash_State_t hReturn = Cus_Flash_WritePage(pPage);
						Cus_Bootloader_FeedIWDG();
						if ( hReturn != CUS_FLASH_OK )
						{
							Cus_BootloaderHook_WriteFailed(pPage->PageAddress, hReturn);
							for( ; ; );
						}

						pPage->Release(pPage);
						g_bootloaderState = BL_STATE_VERIFY_FW;
						break;
					}
				}
				continue;
			}

		case BL_STATE_VERIFY_FW:
			{
				static uint8_t retry_count = 0;
				bool is_FW_VerifyOK = Cus_Flash_VerifyBuffer(APP_START_ADDRESS, (uint8_t *)DOWNLOAD_START_ADDRESS, writeSize);
				Cus_Bootloader_FeedIWDG();
				if ( !is_FW_VerifyOK )
				{
					// Frameware Verify Failed. Start Retry.
					retry_count++;
					if ( retry_count <= 3 )
					{
						is_Retry = true;
						g_bootloaderState = BL_STATE_ERASE_APP;		// Back to BL_STATE_ERASE_APP.
						break;
					}
					else 
					{
						Cus_BootloaderHook_VerifyFailed(APP_START_ADDRESS, writeSize);
					}
					for( ; ; );
				}
				
				is_Retry = false;
				retry_count = 0;
				g_bootloaderState = BL_STATE_CLEAR_IAP_FLAG;
				break;
			}

		case BL_STATE_CLEAR_IAP_FLAG:
			{
				Cus_Bootloader_FeedIWDG();
				uint32_t IAP_Info_Page = Cus_Flash_GetPageStart(IAP_INFO_STRUCT_START_ADDR);
				
				Cus_Flash_State_t eReturn = Cus_Flash_ErasePage(IAP_Info_Page);
				Cus_Bootloader_FeedIWDG();
				if ( eReturn != CUS_FLASH_OK )
				{
					Cus_BootloaderHook_EraseFailed(IAP_INFO_STRUCT_START_ADDR, eReturn);
					for( ; ; );
				}

				g_bootloaderState = BL_STATE_JUMP_APP;
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
					Cus_BootloaderHook_GenericError(BL_STATE_JUMP_APP, IAP_ERRCODE_INVALID_STACKTOPADDR);
					for( ; ; );
				}

				uint32_t reset_vector = *(volatile uint32_t *)(APP_START_ADDRESS + 4);
				if ( reset_vector < APP_START_ADDRESS || reset_vector > (APP_START_ADDRESS + APP_REGION_SIZE) )
				{
					// 复位向量地址非法.
					Cus_BootloaderHook_GenericError(BL_STATE_JUMP_APP, IAP_ERRCODE_INVALID_RESETHANDLER);
					for( ; ; );
				}

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

