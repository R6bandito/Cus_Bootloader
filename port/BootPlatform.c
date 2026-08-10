#include "BootPlatform.h"


/*
 * g_Platform:
 * The abstract ops pointer consumed by the Bootloader core
 * (Bootloader.c / main.c). It is a const pointer to the registered
 * ops table; the core calls platform services (Init / FeedDg /
 * LogOut / PrepareJump) through it, never touching the concrete template
 * implementation directly. Valid only after BootPlatform_Register().
 */
const BootPlatform_Ops_t *g_Platform;

/*
 * gs_ops:
 * The actual memory carrier of the ops table. BootPlatform_Register()
 * value-copies the caller-supplied ops into this static storage and
 * then points g_Platform at it, so a stack-local ops struct at the
 * call site does not become a dangling pointer once the caller returns.
 */
static BootPlatform_Ops_t gs_ops;


void BootPlatform_Register( const BootPlatform_Ops_t *ops )
{
    gs_ops = *ops;
    g_Platform = &gs_ops;
}

