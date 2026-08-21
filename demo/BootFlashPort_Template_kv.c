#include "BootFlashPort_Template_kv.h"
#include "BootFlashPort.h"
#include "Cus_Flash.h"


/* ************************************************** */
int flashInit( void );
static bool flashReadIAP( uint8_t *buf, uint32_t size );
static bool flashClearIAP( void );

static bool flashReadSlot( SlotFlag_Rec_t *out );
static bool flashFlipSlot( const SlotFlag_Rec_t *rec );

bool writeIAP( IAP_Info_t iapReq );
void bootloader_InstallCallbacks( void );

static FlashMgr_Instance_t gs_Mgr_IAP;
static FlashMgr_Instance_t gs_Mgr_Slot;
/* ************************************************** */

/* KV manager regions: shared with the APP via IAP_Protocol.h
   (IAP_MGR_* / SLOT_MGR_*). Do NOT redefine them here. */

static void err_handle( Cus_Flash_State_t Ret )
{
    /* Customize your error handling. */
    (void)Ret;
}


int 
flashInit( void )
{
	Cus_Flash_CalibrateLatency();

	Cus_Flash_State_t hReturn = Cus_FlashMgr_Init( &gs_Mgr_IAP, IAP_MGR_START_ADDR, IAP_MGR_END_ADDR );
	if ( hReturn != CUS_FLASH_OK )
	{
		err_handle( hReturn );
		return -1;
	}

	hReturn = Cus_FlashMgr_Init( &gs_Mgr_Slot, SLOT_MGR_START_ADDR, SLOT_MGR_END_ADDR );
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
		return false;
	}

    /* Get the IAP Record. */
	IAP_Info_t *iapR = (IAP_Info_t *)Out.dataStartAddr;
	memcpy(buf, (uint8_t *)iapR, size);

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


	/* Compact keep-callback: keep only the SLOT record whose seq
	   matches the value passed via @c ctx (the previous newest). */
	static bool 
	KeepCB( const FlashMgr_Record_t *record, void *ctx )
	{
		uint32_t keep_seq = *(uint32_t *)ctx;

		if ( memcmp( record->msgDetail, "SLOT", sizeof("SLOT") ) != 0 )
			return false;

		if ( record->msgSize < sizeof(SlotFlag_Rec_t) )
			return false;

		const SlotFlag_Rec_t *r = (const SlotFlag_Rec_t *)record->msgStartAddr;
		return ( r->seq == keep_seq );
	}


	/* Read the latest valid SLOT record (Manager lookup + validation). */
	static bool 
	flashReadSlot( SlotFlag_Rec_t *out )
	{
		if ( !out )
			return false;

		Cus_Flash_desc_t d;
		Cus_Flash_State_t hReturn = Cus_FlashMgr_GetRecordByDesc( &gs_Mgr_Slot, "SLOT", &d );
		if ( hReturn != CUS_FLASH_OK )
			return false;

		const SlotFlag_Rec_t *r = (const SlotFlag_Rec_t *)d.dataStartAddr;
		if ( r->magic != SLOT_REC_MAGIC )
			return false;

		if ( SlotFlag_RecCrc( r ) != r->crc )
			return false;

		*out = *r;
		return true;
	}

	
	/* Persist one pre-built SLOT record; compact when the area is full. */
	static bool 
	flashFlipSlot( const SlotFlag_Rec_t *rec )
	{
		if ( !rec )
			return false;

		uint32_t free = 0;
		Cus_FlashMgr_GetFreeSpace( &gs_Mgr_Slot, &free );
		if ( free < sizeof(*rec) )
		{
			/* Area full: compact, keeping only the previous newest record. */
			uint32_t keep_seq = rec->seq - 1;
			static uint8_t backBuf[128];
			Cus_FlashMgr_Compact( &gs_Mgr_Slot, KeepCB, &keep_seq, backBuf, sizeof(backBuf) );
		}

		Cus_FlashMgr_Req_t req;
		req.DataBuff = (uint8_t *)rec;
		req.DataSize = sizeof(*rec);
		req.DataType = 0x01;
		memcpy( req.DataDesc, "SLOT", sizeof("SLOT") );
		return ( Cus_FlashMgr_Append( &gs_Mgr_Slot, &req ) == CUS_FLASH_OK );
	}


void 
bootloader_InstallCallbacks( void )
{
	BootFlash_Ops_t Ops = { 0 };
	Ops.ClearIAP 	= flashClearIAP;
	Ops.Init 		= flashInit;
	Ops.ReadIAP 	= flashReadIAP;

	Ops.ReadSlot = flashReadSlot;
	Ops.FlipSlot = flashFlipSlot;

	BootFlash_Register(&Ops);
}



