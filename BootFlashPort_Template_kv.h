#ifndef __CUS_BOOTFLASH_PORT_KV_H__
#define __CUS_BOOTFLASH_PORT_KV_H__


#include <stdint.h>
#include <stdbool.h>
#include "IAP_Protocol.h"
#include "Cus_Flash.h"


/* ************************************************** */
int flashInit( void );
bool writeIAP( IAP_Info_t iapReq );
void bootloader_InstallCallbacks( void );
/* ************************************************** */

#endif /* __CUS_BOOTFLASH_PORT_KV_H__ */

