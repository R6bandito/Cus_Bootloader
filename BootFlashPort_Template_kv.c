#include "BootFlashPort_Template_kv.h"
#include "BootFlashPort.h"



/* ************************************************** */
int flashInit( void );
static int flashErase( uint32_t addr, uint32_t size );
static int flashWrite( uint32_t addr, const uint8_t *data, uint32_t size );
static bool flashReadIAP( uint8_t *buf, uint32_t size );
static bool flashVerify( uint32_t addr, const uint8_t *data, uint32_t size );
static bool flashClearIAP( void );

bool writeIAP( IAP_Info_t iapReq );
void bootloader_InstallCallbacks( void );

static FlashMgr_Instance_t gs_Mgr_IAP;
/* ************************************************** */

/* ************************************************** */
#define 	MANAGER_START_ADDR				(0x08042000UL)
#define 	MANAGER_END_ADDR				(0x08044000UL)
/* ************************************************** */


static void err_handle( Cus_Flash_State_t Ret )
{
    /* Customize your error handling. */
    (void)Ret;
}



int 
flashInit( void )
{
	Cus_Flash_CalibrateLatency();

	Cus_Flash_State_t hReturn = Cus_FlashMgr_Init(&gs_Mgr_IAP, MANAGER_START_ADDR, MANAGER_END_ADDR);
	if ( hReturn != CUS_FLASH_OK )
	{
		err_handle( hReturn );
		return -1;
	}

	/* This implementation assumes the default DWT timer. */
	/* If you have overridden the timebase API externally, you may comment out this line. */
	/* Leaving it uncommented is also safe; DWT will be used without any side effects. */
	Cus_Flash_SYS_TickInit();

	return 0;
}



static int 
flashErase( uint32_t addr, uint32_t size )
{
	#if (DEVICE_STM32F1xx)
		uint32_t waitErasedPageCounts = (size / CUS_FLASH_BYTE_PER_PAGE);
		uint32_t remaining = (size % CUS_FLASH_BYTE_PER_PAGE);

		if ( remaining > 0 )
			waitErasedPageCounts++;
	
		uint32_t pageAddr = Cus_Flash_GetPageStart(addr);
		int16_t hReturn = Cus_Flash_ErasePages(pageAddr, waitErasedPageCounts);
		if ( (hReturn < 0) || (hReturn != waitErasedPageCounts) )
			return -1;
	#endif /* DEVICE_STM32F1xx */

    #if (DEVICE_STM32F4xx)
        const Cus_Flash_Sector_t *pSector = Cus_Flash_GetSectorbyAddr(addr);
        if (!pSector)
            return -1;
    
        uint32_t current = addr;
        uint32_t end = addr + size;
    
        while (current < end)
        {
            pSector = Cus_Flash_GetSectorbyAddr(current);
            if (!pSector) break;
    
            Cus_Flash_State_t ret = Cus_Flash_EraseSector(pSector);
            if (ret != CUS_FLASH_OK)
                return -1;
    
            current = pSector->secStartAddr + pSector->secSize;
        }
    #endif

	return 0;
}



static int 
flashWrite( uint32_t addr, const uint8_t *data, uint32_t size )
{
	if ( !data || (size == 0) )
		return -1;

	Cus_Flash_State_t hReturn = Cus_Flash_WriteBuffer(addr, data, size);
	if ( hReturn != CUS_FLASH_OK )
	{
		err_handle(hReturn);
		return -2;
	}

	return 0;
}



static bool 
flashReadIAP( uint8_t *buf, uint32_t size )
{
	if ( !buf || size == 0 )
		return false;

	/* Read the latest IAP record. */
	Cus_Flash_desc_t Out;
	Cus_Flash_State_t hReturn = Cus_FlashMgr_GetRecordByDesc(&gs_Mgr_IAP, "IAP_Req", &Out);
	if ( hReturn != CUS_FLASH_OK )
	{
		/* No IAP record get. Return F. */
		err_handle(hReturn);
		return false;
	}

    /* Get the IAP Record. */
	IAP_Info_t *iapR = (IAP_Info_t *)Out.dataStartAddr;
	memcpy(buf, (uint8_t *)iapR, size);

	return true;
}



static bool 
flashVerify( uint32_t addr, const uint8_t *data, uint32_t size )
{
	if ( !data || size == 0 )
		return false;

	bool isVerified = Cus_Flash_VerifyBuffer(addr, data, size);
	if ( !isVerified )
	{
		/* Dismatch detected. Return F. */
		return false;
	}

	return true;
}



static bool 
flashClearIAP( void )
{
	Cus_Flash_State_t hReturn = Cus_FlashMgr_DeleteByDesc(&gs_Mgr_IAP, "IAP_Req");
	if ( (hReturn != CUS_FLASH_OK) && (hReturn != CUS_FLASH_NOT_FOUND) )
	{
		/* Something Err happended. Inform and Return. */
		err_handle(hReturn);
		return false;
	}

	return true;
}



bool 
writeIAP( IAP_Info_t iapReq )
{
	/* Check if the remaining free space is sufficient for the next IAP record. */
	uint32_t Remain = 0;
	Cus_Flash_State_t hReturn = Cus_FlashMgr_GetFreeSpace(&gs_Mgr_IAP, &Remain);
	if ( hReturn != CUS_FLASH_OK )
		goto ERROR;

	if ( Remain < sizeof(iapReq) )
	{
		/* No enough space. Erase region to release space. */
		hReturn = Cus_FlashMgr_EraseRegion(&gs_Mgr_IAP);
		if ( hReturn != CUS_FLASH_OK )
			goto ERROR;
	}

	/* Build the IAP info and append the message. */
	Cus_FlashMgr_Req_t Req;
	Req.DataBuff = (uint8_t *)&iapReq;
	Req.DataSize = sizeof(iapReq);
	Req.DataType = 0x01;
	memcpy(Req.DataDesc, "IAP_Req", sizeof("IAP_Req"));

	hReturn = Cus_FlashMgr_Append(&gs_Mgr_IAP, &Req);
	if ( hReturn != CUS_FLASH_OK )
		goto ERROR;

	return true;

	ERROR:
	err_handle(hReturn);
	return false;
}



void 
bootloader_InstallCallbacks( void )
{
	BootFlash_Ops_t Ops;
	Ops.ClearIAP 	= flashClearIAP;
	Ops.Erase 		= flashErase;
	Ops.Init 		= flashInit;
	Ops.ReadIAP 	= flashReadIAP;
	Ops.Verify 		= flashVerify;
	Ops.Write 		= flashWrite;

	BootFlash_Register(&Ops);
}




