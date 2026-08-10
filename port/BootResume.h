#ifndef __BOOT_RESUME_H__
#define __BOOT_RESUME_H__

#include <stdint.h>
#include <stdbool.h>


typedef struct 
{
    uint16_t magic;
    uint16_t state;
    uint16_t packs;

} BootResume_Data_t;


typedef bool (*BootResume_SaveFn) ( const BootResume_Data_t *data );
typedef bool (*BootResume_LoadFn) ( BootResume_Data_t *data );
typedef void (*BootResume_ClearFn)( void );

typedef struct 
{
    BootResume_SaveFn  Save;
    BootResume_LoadFn  Load;
    BootResume_ClearFn Clear;

} BootResume_Ops_t;

extern const BootResume_Ops_t *g_BootResume;

void BootResume_Register( const BootResume_Ops_t *ops );

#endif
