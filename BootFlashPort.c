#include "BootFlashPort.h"

static const BootFlash_Ops_t *gs_ops;

const BootFlash_Ops_t *g_BootFlash;

void BootFlash_Register( const BootFlash_Ops_t *ops )
{
    gs_ops = ops;
    g_BootFlash = gs_ops;
}
