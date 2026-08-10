/*
 * BootloaderConf.h -- BOOTLOADER PROJECT PRIVATE configuration.
 * Not shared with the APP project. Everything the APP must know
 * (memory layout, request format, A/B records) lives in IAP_Protocol.h.
 */
#ifndef __BOOTLOADER_CONF_H__
#define __BOOTLOADER_CONF_H__



/* -------------- Feature ------------------- */

	/* Enable debug output (BL_Log / BL_LogF). 0=disabled. 1=enabled. */
	#define USE_DEBUG               (1)

	/* Enable the DFU APP (user's minimal transport layer for firmware
	   download). 0=disabled. 1=enabled. */
	#define USE_DFU_APP             (0)

	/* Enable power-fail resume. 0=disabled: after a power loss during the
	   update, the next boot restarts the whole flash procedure.
	   1=enabled: the next boot continues from the recorded pack. */
	#define USE_POWER_FAIL_RESUME   (0)

	/* Take over IWDG feeding. 0=disabled: no watchdog handling during
	   long operations. 1=enabled: long operations feed the watchdog
	   automatically. */
	#define USE_DG                  (0)

	/* A/B dual-slot OTA. 0=legacy single-slot update. 1=dual-slot. */
	#define USE_AB_SLOT             (0)

/* ------------------------------------------ */

/* -------------- Core Config ------------------- */

  /* SRAM range used to validate the APP stack top before the jump. */
  #define MCU_SRAM_BASE_ADDR                (SRAM_BASE)
  #define MCU_SRAM_SIZE                     (64 * 1024)

  /* Firmware is transferred in fixed-size packs (protocol-internal
     granularity; the user side is not required to follow it). */
  #define BYTES_PER_PACKS                   (1024UL)

/* ------------------------------------------ */

#if (USE_AB_SLOT) && (USE_POWER_FAIL_RESUME)
	#error " A/B IAP cant use USE_POWER_FAIL_RESUME. "
#endif 


  #if (USE_DFU_APP)
    #define DFU_APP_START_ADDR              (0x0807C000UL)
    #define DFU_APP_REGION_SIZE             (0x00004000UL)   // 16kb
  #endif // USE_DFU_APP

/* ------------------------------------------ */

#endif 
