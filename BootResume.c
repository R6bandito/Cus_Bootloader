#include "BootResume.h"

const BootResume_Ops_t *g_BootResume;

static BootResume_Ops_t gs_ops;

void BootResume_Register( const BootResume_Ops_t *ops )
{
    /* Value-copy the caller-supplied ops so that a local
       variable at the call site does not become a dangling
       pointer once the caller returns. */
    gs_ops = *ops;
    g_BootResume = &gs_ops;
}
