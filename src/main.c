#include "main.h"
#include "BootPlatform.h"


/* ---------------------------------------------- */
uint8_t wBuf[BYTES_PER_PACKS];
extern volatile BL_State_t g_bootloaderState;
static bool is_Retry = false;

/* Jump target: active slot in A/B mode; plain APP region in legacy mode. */
static uint32_t gs_run_addr = APP_START_ADDRESS;
static uint32_t gs_size = APP_REGION_SIZE;

#if (USE_POWER_FAIL_RESUME)
	extern BootResume_Data_t LoadConf;
	static uint8_t SavedSTA;
	static uint8_t ResumeSaveErr;		/* Set once a Save fails: resume is no longer reliable. */

	/* Wrapper for the power-fail resume record save.
	   On the first failure: logs a WARN, clears the stored record so the
	   next boot starts a full update, and disables all further saves for
	   this run (prevents log flooding and repeated Flash writes). */
	static void ResumeSave( void )
	{
		if ( ResumeSaveErr ) return;	/* Already disabled for this run. */

		if ( !g_BootResume->Save( &LoadConf ) )
		{
			ResumeSaveErr = 1;
			BL_Log( "[WARN] RESUME SAVE FAILED. RESUME DISABLED FOR THIS RUN.\n", 1 );
			g_BootResume->Clear();
		}
	}
#endif // USE_POWER_FAIL_RESUME
/* ---------------------------------------------- */


int main( void )
{
	BL_Log("============== Bootloader Running ==============", 0);

	Cus_Bootloader_Init();

	#if (USE_DFU_APP)
		/* User-defined DFU trigger: return true to jump into the DFU APP
		   (user's minimal transport layer for firmware download).
		   If the DFU APP is not programmed / corrupt, JumpToDFUAPP
		   returns false and the normal startup flow continues. */
		BL_ErrCode_t err = 0;
		if ( g_Platform->CheckDFU && g_Platform->CheckDFU() )
		{
			if ( !Cus_Bootloader_JumpToDFUAPP( &err ) )
			{
				/* DFU enter false.  */
				Cus_BootloaderHook_DFUEnterFailed( BL_STATE_START, (uint32_t)err );
			}
		}
	#endif /* USE_DFU_APP */

	g_bootloaderState = BL_STATE_START;

	uint8_t uReturn = Cus_Bootloader_CheckIAPRequest();
	if ( !uReturn )		
		/* No need to update. Jump to the APP. */ 
		g_bootloaderState = BL_STATE_JUMP_APP;
	else 
		#if (USE_AB_SLOT)
			/* A/B: the firmware is already written to the target slot by
			   the user side; verify it there, then flip. */
			g_bootloaderState = BL_STATE_VERIFY_AB;
		#else
			g_bootloaderState = BL_STATE_ERASE_APP;
		#endif

	#if (USE_POWER_FAIL_RESUME)
		bool isLoad = g_BootResume->Load( &LoadConf );
		if ( isLoad )
		{
			/* Restore the saved state and resume from there. */ 
			g_bootloaderState = LoadConf.state;
			SavedSTA = 1;
		}
		else 
		{
			/* Saved states load error. POWER_FAIL_RESUME Not avaliable. */
			BL_Log( "[WARN] RESUME LOAD FAILED. RESTART FULL UPDATE.\n", 1 );
		}
	#endif /* USE_POWER_FAIL_RESUME */ 

	#if (USE_AB_SLOT)
		SlotFlag_Rec_t current_slot;
		if ( !g_BootFlash->ReadSlot( &current_slot ) )
		{
			current_slot.active = SLOT_FLAG_MAGIC_A;
			current_slot.seq = 0;
		}

		gs_run_addr = BootSlot_MagicToAddr( current_slot.active );
	#endif /* USE_AB_SLOT */

	#if (!USE_AB_SLOT)
		/* Firmware update information. */ 
		IAP_Info_t iap_info;
		g_BootFlash->ReadIAP( (uint8_t *)&iap_info, sizeof(iap_info) );
		uint32_t writeSize = iap_info.app_size;
		uint32_t total_packs = (writeSize / BYTES_PER_PACKS);
		uint32_t remaining = (writeSize % BYTES_PER_PACKS);
	#endif 

	while(1)
	{
		switch (g_bootloaderState)
		{
			#if (!USE_AB_SLOT)
			case BL_STATE_ERASE_APP:
			{
				#if (USE_POWER_FAIL_RESUME)
					/* Record the BL_STATE_ERASE_APP state. */
					LoadConf.state = BL_STATE_ERASE_APP;			
					ResumeSave();
				#endif 

				int hReturn = g_BootFlash->Erase( APP_START_ADDRESS, APP_REGION_SIZE );
				if ( hReturn < 0 )
				{
					Cus_BootloaderHook_EraseFailed( APP_START_ADDRESS, hReturn );
					for( ; ; );
				}
				g_bootloaderState = BL_STATE_WRITE_FW;

				#if (USE_POWER_FAIL_RESUME)
					/* Record the BL_STATE_WRITE_FW state. */
					LoadConf.state = BL_STATE_WRITE_FW;
					ResumeSave();
				#endif 

				break;
			}
			#endif /* USE_AB_SLOT */

			#if (!USE_AB_SLOT)
			case BL_STATE_WRITE_FW:
			{
				static uint16_t current_packs = 0;
				static uint32_t current_downloadAddr = DOWNLOAD_START_ADDRESS;
				static uint32_t current_appAddr = APP_START_ADDRESS;

				#if (USE_POWER_FAIL_RESUME)
					static uint8_t resume_initialize = 0;
					if ( !resume_initialize && SavedSTA )
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
									ResumeSave();
								#endif 
								continue;
							}
							else if ( (current_packs == total_packs) && remaining )
							{
								goto FLAG1;
							}
						}

						printf("\nPacks: %d, downloadAddr: %x, appAddr: %x\n", current_packs, current_downloadAddr, current_appAddr);
						resume_initialize = 1;		// Non-reentrant flag.
					}
					else 
					{
						resume_initialize = 1;
					}
				#endif 

				if ( is_Retry )
				{
					current_packs = 0;
					current_downloadAddr = DOWNLOAD_START_ADDRESS;
					current_appAddr = APP_START_ADDRESS;
				}

				memcpy( wBuf, (uint8_t *)current_downloadAddr, BYTES_PER_PACKS );

				int hReturn = g_BootFlash->Write( current_appAddr, wBuf, BYTES_PER_PACKS );
				if ( hReturn < 0 )
				{
					/* TODO. */
					Cus_BootloaderHook_WriteFailed( current_appAddr, hReturn );
				}

				#if (USE_POWER_FAIL_RESUME)
					/* Pack written successfully. Update the record. */
					LoadConf.packs++;
					ResumeSave();
				#endif 

				/* Next packs. */
				current_packs++;	

				/* Advance to the next pack to read. */
				current_downloadAddr += BYTES_PER_PACKS;

				/* Advance to the next pack to write. */
				current_appAddr += BYTES_PER_PACKS;

FLAG1:
				if ( current_packs == total_packs )
				{	
					if ( remaining != 0 && remaining < BYTES_PER_PACKS )
					{
						/* Remaining data is less than one pack. */ 
						memset( wBuf, 0, BYTES_PER_PACKS );
						memcpy( wBuf, (uint8_t *)current_downloadAddr, remaining );
						hReturn = g_BootFlash->Write( current_appAddr, wBuf, remaining );
						if ( hReturn < 0 )
						{
							Cus_BootloaderHook_WriteFailed( current_appAddr, hReturn );
						}
					}

					/* DOWNLOAD area fully written to APP. Update the state machine. */
					g_bootloaderState = BL_STATE_VERIFY_FW;

					#if (USE_POWER_FAIL_RESUME)
						/* Save PowerResume Conf. */
						LoadConf.state = BL_STATE_VERIFY_FW;
						ResumeSave();
					#endif 

					break;	
				}
				continue;
			}
			#endif /* USE_AB_SLOT */

		#if (!USE_AB_SLOT)
		case BL_STATE_VERIFY_FW:
			{
				static uint8_t retry_count = 0;
				bool is_FW_VerifyOK = g_BootFlash->Verify( APP_START_ADDRESS, (uint8_t *)DOWNLOAD_START_ADDRESS, writeSize );
				if ( !is_FW_VerifyOK )
				{
					/* Frameware Verify Failed. Start Retry. */
					retry_count++;
					if ( retry_count <= 3 )
					{
						is_Retry = true;

						#if (USE_POWER_FAIL_RESUME)
							/* Clear the previous counter before retry to keep the power-loss state consistent with the Bootloader. */
							g_BootResume->Clear();
						#endif 

						/* Back to BL_STATE_ERASE_APP. */
						g_bootloaderState = BL_STATE_ERASE_APP;		
						break;
					}
					else 
					{
						// All 3 retries failed. Trigger the VerifyFailed hook (default: soft reset).
						Cus_BootloaderHook_VerifyFailed( APP_START_ADDRESS, writeSize );
					}
				}
				
				is_Retry = false;
				retry_count = 0;
				g_bootloaderState = BL_STATE_CLEAR_IAP_FLAG;

				#if (USE_POWER_FAIL_RESUME)
					LoadConf.state = BL_STATE_CLEAR_IAP_FLAG;
					ResumeSave();
				#endif 

				break;
			}
			#endif /* USE_AB_SLOT */

		#if (USE_AB_SLOT)
		case BL_STATE_VERIFY_AB:
			{
				/* Resolve the slot state once. */
				SlotFlag_Rec_t cur;
				if ( !g_BootFlash->ReadSlot( &cur ) )
				{
					cur.active = SLOT_FLAG_MAGIC_A;
					cur.seq    = 0;
				}
				uint32_t target = (cur.active == SLOT_FLAG_MAGIC_A)
								? APP_SLOT_B_START : APP_SLOT_A_START;

				/* The IAP request carries the authoritative CRC. */
				uint8_t buf[sizeof(IAP_Info_t)] = { 0 };
				g_BootFlash->ReadIAP( buf, sizeof(buf) );
				IAP_Info_t *iap = (IAP_Info_t *)buf;
				gs_size = iap->app_size;

				/* Re-compute CRC32 over the target slot and compare. */
				uint32_t calc = Cus_Bootloader_CRC32Caculate( (uint8_t *)target, iap->app_size );
				if ( calc != iap->CRC32 )
				{
					/* Verify failed: do NOT flip, keep the current slot. */
					BL_Log( "[WARN] TARGET SLOT VERIFY FAILED. KEEP CURRENT.\n", 1 );
					g_bootloaderState = BL_STATE_CLEAR_IAP_FLAG;
					break;
				}

				BL_Log( "[INFO] FIRMWARE VERIFY PASS.\n", 0 );

				/* Verified: flip the slot flag, the target becomes active. */
				SlotFlag_Rec_t next;
				next.magic  = SLOT_REC_MAGIC;
				next.active = (cur.active == SLOT_FLAG_MAGIC_A) ? SLOT_FLAG_MAGIC_B : SLOT_FLAG_MAGIC_A;
				next.seq    = cur.seq + 1;
				next.crc    = SlotFlag_RecCrc( &next );

				if ( g_BootFlash->FlipSlot( &next ) )
				{
					gs_run_addr = BootSlot_MagicToAddr( next.active );
					g_bootloaderState = BL_STATE_CLEAR_IAP_FLAG;
				}
				else
				{
					/* Flip failed: stay on the pre-flip slot. */
					BL_Log( "[WARN] SLOT FLIP FAILED. STAY ON CURRENT PARTITION. RETRY ON NEXT BOOT\n", 1 );
					g_bootloaderState = BL_STATE_JUMP_APP;
				}
				break;
			}
		#endif /* USE_AB_SLOT */

		case BL_STATE_CLEAR_IAP_FLAG:
			{
				int eReturn = g_BootFlash->ClearIAP();
				if ( eReturn < 0 )
				{
					BL_Log( "[WARN] IAP REQUEST CLEAR FAILED. WILL RETRY ON NEXT BOOT.\n", 1 );
				}

				g_bootloaderState = BL_STATE_JUMP_APP;

				#if (USE_POWER_FAIL_RESUME)
					LoadConf.state = BL_STATE_JUMP_APP;
					ResumeSave();
				#endif 				

				break;
			}

		case BL_STATE_JUMP_APP:
			{
				/* Jump to APP. */
				/* Read the stack top address. */
				uint32_t msp = *(volatile uint32_t *)gs_run_addr;		
				uint32_t reset_vector = *(volatile uint32_t *)(gs_run_addr + 4);

				if ( msp < MCU_SRAM_BASE_ADDR || msp > (MCU_SRAM_BASE_ADDR + MCU_SRAM_SIZE)
						|| reset_vector < gs_run_addr || reset_vector > gs_run_addr + gs_size )
				{
					/* Invalid stack top address. */
					#if (USE_AB_SLOT)
						/* Back to the other slot when the run slot is not executable. */
						SlotFlag_Rec_t cur;
						if ( !g_BootFlash->ReadSlot( &cur ) )
						{
							BL_Log( "[ERROR] STACK ADDR ERR AND BACK TO OTHER ERROR.\n", 2 );
							BL_ASSERT( 0 );
						}

						uint32_t otherAddr = (cur.active == SLOT_FLAG_MAGIC_A) ? APP_SLOT_B_START : APP_SLOT_A_START;
						uint32_t otherMSP = *(volatile uint32_t *)otherAddr;
						uint32_t otherVEC = *(volatile uint32_t *)(otherAddr + 4);

						/* Validate the other slot's vector table before committing. */
						if ( otherMSP < MCU_SRAM_BASE_ADDR || otherMSP > (MCU_SRAM_BASE_ADDR + MCU_SRAM_SIZE)
								||  otherVEC  < otherAddr || otherVEC  > (otherAddr + APP_REGION_SIZE) )
						{
							BL_Log( "[ERROR] OTHER PARTITION STACK ALSO ERROR.\n", 2 );
							BL_ASSERT( 0 );
						}

						/* Flip the flag back to the other slot. */
						SlotFlag_Rec_t next;
						next.active = (cur.active == SLOT_FLAG_MAGIC_A) ? SLOT_FLAG_MAGIC_B : SLOT_FLAG_MAGIC_A;
						next.magic = SLOT_REC_MAGIC;
						next.seq = (cur.seq + 1);
						next.crc = SlotFlag_RecCrc( &next );
						if ( !g_BootFlash->FlipSlot( &next ) )
						{
							/* Flip Err? */
							BL_Log( "[ERROR] OTHER PARTITION FLIP ERROR.\n", 2 );
							BL_ASSERT( 0 );
						}

						gs_run_addr = otherAddr;

						/* Reload the stack top for the rolled-back slot (the
						   old msp still holds the invalid value). */
						msp = otherMSP;
						reset_vector = otherVEC;

						/* End this update attempt: the IAP request is stale
						   now (the new firmware is not executable), otherwise
						   the next boot would re-verify / re-flip / re-fail. */
						g_BootFlash->ClearIAP();

						/* Rollback OK: gs_run_addr / msp / reset_vector are
						   updated -- continue to the jump sequence below. */
					#else /* !USE_AB_SLOT */
						/* Single-slot fallback: the firmware itself has a bad
						   vector table (verify passed, so this is a FW config
						   error, not a transfer fault). */
						BL_Log( "[ERROR] FIRMWARE STACK TOP OR VECTOR INVALID (FW CONFIG ERROR).\n", 1 );
						BL_ASSERT( 0 );
					#endif /* USE_AB_SLOT */
				}

				#if (USE_POWER_FAIL_RESUME)
					/* Clear the resume flag before the final jump to APP. */
					g_BootResume->Clear();		
				#endif 

				/* Platform pre-jump hook: shut down SysTick, clear pending
				   interrupts, de-init peripherals, etc. Runs with interrupts
				   still enabled so the implementation may use HAL_Delay. */
				if ( g_Platform->PrepareJump )
					g_Platform->PrepareJump();

				{
					/* ==========================================================
					* ARCHITECTURE-SPECIFIC JUMP SEQUENCE
					* ----------------------------------------------------------
					* This block is the core of the application jump and is tied
					* to the Cortex-M architecture:
					*   1. __disable_irq()  -- stop interrupts during the switch.
					*   2. SCB->VTOR        -- relocate the vector table to the
					*                          target image. NOTE: Cortex-M0/M0+
					*                          have NO VTOR (the vector table is
					*                          fixed at 0x00000000); those targets
					*                          must be linked there or use a RAM
					*                          remap instead.
					*   3. __DSB()          -- make the VTOR write effective
					*                          before any exception can fire.
					*   4. __set_MSP(msp)   -- load the target image's initial
					*                          stack pointer (Cortex-M only).
					* The actual jump (app_entry()) then calls the target's
					* reset vector. When porting to another architecture
					* (RISC-V, ARM7, ...), this whole block must be re-implemented
					* according to the target's boot convention; keep it isolated
					* here on purpose.
					* ========================================================== */
					__disable_irq();

					/* Relocate the vector table. */
					SCB->VTOR = gs_run_addr;
					__DSB();
	
					/* Set the main stack pointer. */
					__set_MSP(msp);
				}

				void (*app_entry)(void) = (void (*)(void))reset_vector;

				/* Jump to APP. */
				app_entry();

				/* Normally the program should never reach here. */
				BL_ASSERT( 0 );
			}
		
		default:	break;
		}
	}
}

