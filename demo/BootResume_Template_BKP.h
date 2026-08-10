#ifndef __CUS_BOOT_TEP_BKP_H__
#define __CUS_BOOT_TEP_BKP_H__


#include <stdint.h>
#include <stdbool.h>
#include "BootResume.h"
#include "stm32f1xx.h"


/* ******************************************************** */
#define RESUME_BKP_MAGIC_ADDR           (BKP_BASE + 0x0C)   // BKP register offset (first two used by DFU APP).
#define RESUME_BKP_STATE_ADDR           (BKP_BASE + 0x10)   
#define RESUME_BKP_PACK_ADDR            (BKP_BASE + 0x14)

#define PWRFAIL_RESUME_BKP_MAGIC        (0xBABAUL)   // Validation magic.
/* ******************************************************** */


void Cus_Boot_RecordInit( void );
void Cus_Boot_Clearcb( void );
bool Cus_Boot_Savecb( const BootResume_Data_t *data );
bool Cus_Boot_Loadcb( BootResume_Data_t *data );

/* Convenience wrapper: init BKP domain + register callbacks in one call. */
void BootResume_bkp_Install( void );


#endif /* __CUS_BOOT_TEP_BKP_H__  */
