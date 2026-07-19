#include "BootResume_Template_kv.h"
#include "BootResume.h"
#include "Cus_Flash.h"


/* ************************************ */
#define RESUME_MGR_START  (0x08078000UL)
#define RESUME_MGR_END    (0x0807A000UL)
/* ************************************ */

static FlashMgr_Instance_t gs_ResumeMgr;



static bool 
kvResume_Save( const BootResume_Data_t *data )
{
    Cus_FlashMgr_Req_t req = {
        .DataBuff = (uint8_t *)data,
        .DataSize = sizeof(*data),
        .DataType = 0x01,
    };
    memcpy(req.DataDesc, "RESUME", sizeof("RESUME"));

    return (Cus_FlashMgr_Append(&gs_ResumeMgr, &req) == CUS_FLASH_OK);
}



static bool 
kvResume_Load( BootResume_Data_t *data )
{
    Cus_Flash_desc_t out;
    if (Cus_FlashMgr_GetRecordByDesc(&gs_ResumeMgr, "RESUME", &out) != CUS_FLASH_OK)
        return false;

    memcpy(data, (void *)out.dataStartAddr, sizeof(*data));
    return true;
}



static void 
kvResume_Clear( void )
{
    Cus_FlashMgr_DeleteAllByDesc(&gs_ResumeMgr, "RESUME", NULL);
    Cus_FlashMgr_EraseRegion(&gs_ResumeMgr);
}



void 
BootResume_kv_Install( void )
{
    Cus_FlashMgr_Init(&gs_ResumeMgr, RESUME_MGR_START, RESUME_MGR_END);

    static const BootResume_Ops_t ops = {
        .Save  = kvResume_Save,
        .Load  = kvResume_Load,
        .Clear = kvResume_Clear,
    };

    BootResume_Register(&ops);
}
