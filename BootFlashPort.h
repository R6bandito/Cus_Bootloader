#ifndef __BOOT_FLASH_PORT_H__
#define __BOOT_FLASH_PORT_H__

#include <stdint.h>
#include <stdbool.h>

typedef int  (*BootFlash_InitFn)  ( void );
typedef int  (*BootFlash_EraseFn) ( uint32_t addr, uint32_t size );
typedef int  (*BootFlash_WriteFn) ( uint32_t addr, const uint8_t *data, uint32_t size );
typedef bool (*BootFlash_ReadFn)  ( uint32_t addr, uint8_t *buf, uint32_t size );
typedef bool (*BootFlash_VerifyFn)( uint32_t addr, const uint8_t *data, uint32_t size );


typedef struct {
    BootFlash_InitFn   Init;
    BootFlash_EraseFn  Erase;
    BootFlash_WriteFn  Write;
    BootFlash_ReadFn   ReadIAP;
    BootFlash_VerifyFn Verify;

} BootFlash_Ops_t;


extern const BootFlash_Ops_t *g_BootFlash;

void BootFlash_Register( const BootFlash_Ops_t *ops );

#endif
