#include "BootResume.h"

static const BootResume_Ops_t *gs_ops;

const BootResume_Ops_t *g_BootResume;

void BootResume_Register( const BootResume_Ops_t *ops )
{
    gs_ops = ops;
    g_BootResume = gs_ops;
}

