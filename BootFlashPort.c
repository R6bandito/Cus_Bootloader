#include "BootFlashPort.h"

const BootFlash_Ops_t *g_BootFlash;

static BootFlash_Ops_t gs_ops;

void BootFlash_Register( const BootFlash_Ops_t *ops )
{
    gs_ops = *ops;
    g_BootFlash = &gs_ops;
}
