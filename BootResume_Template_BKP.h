#ifndef __CUS_BOOT_TEP_BKP_H__
#define __CUS_BOOT_TEP_BKP_H__


#include <stdint.h>
#include <stdbool.h>
#include "BootResume.h"
#include "stm32f1xx.h"


/* ******************************************************** */
#define RESUME_BKP_MAGIC_ADDR           (BKP_BASE + 0x0C)   // 偏移两个BKP寄存器(前两个寄存器用于USE_RECOVERY_APP功能).
#define RESUME_BKP_STATE_ADDR           (BKP_BASE + 0x10)   
#define RESUME_BKP_PACK_ADDR            (BKP_BASE + 0x14)

#define PWRFAIL_RESUME_BKP_MAGIC        (0xBABAUL)   // 验证魔数.
/* ******************************************************** */


void Cus_Boot_RecordInit( void );
void Cus_Boot_Clearcb( void );
bool Cus_Boot_Savecb( const BootResume_Data_t *data );
bool Cus_Boot_Loadcb( BootResume_Data_t *data );



#endif /* __CUS_BOOT_TEP_BKP_H__  */
