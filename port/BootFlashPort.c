#include "BootFlashPort.h"


/*
 * g_BootFlash:
 * The abstract ops pointer consumed by the Bootloader core
 * (Bootloader.c / main.c). It is a const pointer to the registered
 * ops table; the core calls flash services (Init / Erase / Write /
 * ReadIAP / ClearIAP / Verify) through it, never touching the concrete
 * template implementation directly. Valid only after
 * BootFlash_Register().
 */
const BootFlash_Ops_t *g_BootFlash;

/*
 * gs_ops:
 * The actual memory carrier of the ops table. BootFlash_Register()
 * value-copies the caller-supplied ops into this static storage and
 * then points g_BootFlash at it, so a stack-local ops struct at the
 * call site does not become a dangling pointer once the caller returns.
 */
static BootFlash_Ops_t gs_ops;

void BootFlash_Register( const BootFlash_Ops_t *ops )
{
    gs_ops = *ops;
    g_BootFlash = &gs_ops;
}

