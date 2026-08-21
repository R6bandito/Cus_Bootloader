#include "main.h"
#include "BootPlatform.h"


/* ---------------------------------------------- */
extern volatile BL_State_t g_bootloaderState;

/* Jump target: the active slot (A/B mode). */
static uint32_t gs_run_addr = APP_SLOT_A_START;
static uint32_t gs_size = APP_REGION_SIZE;
/* ---------------------------------------------- */


int main( void )
{
	Cus_Bootloader_Init();

	BL_Log("============== Bootloader Running ==============\n", 0);

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
		/* A/B: the firmware is already written to the target slot by the
		   user side; verify it there, then flip. */
		g_bootloaderState = BL_STATE_VERIFY_AB;

	SlotFlag_Rec_t current_slot;
	if ( !g_BootFlash->ReadSlot( &current_slot ) )
	{
		current_slot.active = SLOT_FLAG_MAGIC_A;
		current_slot.seq = 0;
	}

	gs_run_addr = BootSlot_MagicToAddr( current_slot.active );

	while(1)
	{
		switch (g_bootloaderState)
		{
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

		case BL_STATE_CLEAR_IAP_FLAG:
			{
				int eReturn = g_BootFlash->ClearIAP();
				if ( eReturn < 0 )
				{
					BL_Log( "[WARN] IAP REQUEST CLEAR FAILED. WILL RETRY ON NEXT BOOT.\n", 1 );
				}

				g_bootloaderState = BL_STATE_JUMP_APP;

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
				}

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

