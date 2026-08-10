#include "BootPlatform_stm32f1_Template.h"
#include "BootPlatform.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>


/* ************************************ */
static void stm32f1_Init( void );
static void stm32f1_PrepareJump( void );
static void Cus_Bootloader_Utils_SystemClockConfig( void );
static void err_handle( void );
/* ************************************ */

#if (USE_DFU_APP)
	/* User-defined DFU trigger pin (adjust to your hardware). */
	#define DFU_TRIG_GPIO_Port		GPIOA
	#define DFU_TRIG_Pin			GPIO_PIN_0
	static bool stm32f1_CheckDFU( void );
#endif /* USE_DFU_APP */

#if (USE_DG)
	static void stm32f1_FeedIWDG( void );
#endif /* USE_DG */
/* ************************************ */

/* Central platform error handler: fatal HAL failure (clock / UART
init cannot recover). Halts forever; place a breakpoint here while
debugging. If USE_DG is enabled and a watchdog reset is unwanted,
feed the dog inside the loop. */
static void 
err_handle( void )
{
	for( ; ; )
	{
		/* Halt. */
	}
}

#if (USE_DEBUG)
	/* ---------------------------------- */
	static UART_HandleTypeDef huart_debug;
	/* ---------------------------------- */

	#if (__ARMCC_VERSION >= 6010050)            /* 使用AC6编译器时 */
		__asm(".global __use_no_semihosting\n\t");  /* 声明不使用半主机模式 */
		__asm(".global __ARM_use_no_argv \n\t");    /* AC6下需要声明main函数为无参数格式，否则部分例程可能出现半主机模式 */
	#else
		/* 使用AC5编译器时, 要在这里定义__FILE 和 不使用半主机模式 */
		#pragma import(__use_no_semihosting)
	#endif

	struct __FILE
	{
		int handle;
			
	};
	FILE __stdout;

	int 
	_ttywrch(int ch)
	{
		ch = ch;
		return ch;
	}

	void 
	_sys_exit(int x)
	{
		x = x;
	}

	char *
	_sys_command_string(char *cmd, int len)
	{
		return NULL;
	}

	/* 重定向 printf. */
	int 
	fputc(int ch, FILE *f) 
	{
		(void)f;
		if ( HAL_UART_Transmit(&huart_debug, (uint8_t*)&ch, 1, 500) != HAL_OK )
		{
			err_handle();
		}

		return ch;
	}


	static void 
	Cus_Bootloader_Utils_Debug_UART_Init( void )
	{
		__HAL_RCC_GPIOA_CLK_ENABLE();

		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStructure.Pin = GPIO_PIN_9 | GPIO_PIN_10;
		GPIO_InitStructure.Pull = GPIO_PULLUP;
		GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

		__HAL_RCC_USART1_CLK_ENABLE();

		huart_debug.Instance = USART1;
		huart_debug.Init.BaudRate = 115200;
		huart_debug.Init.Mode = UART_MODE_TX_RX;
		huart_debug.Init.Parity = UART_PARITY_NONE;
		huart_debug.Init.StopBits = UART_STOPBITS_1;
		huart_debug.Init.WordLength = UART_WORDLENGTH_8B;

		if ( HAL_UART_Init(&huart_debug) != HAL_OK )
		{
			err_handle();
		}
	}

#endif /* USE_DEBUG */


static void 
stm32f1_Init( void )
{
    HAL_Init();
    Cus_Bootloader_Utils_SystemClockConfig();

	#if (USE_DEBUG)
		Cus_Bootloader_Utils_Debug_UART_Init();
	#endif /* USE_DEBUG */
}


/* Shut down the kernel / HAL environment before jumping to the next image. */
static void 
stm32f1_PrepareJump( void )
{
    SysTick->CTRL = 0;                    /* Disable SysTick. */
    SysTick->LOAD = 0;                    /* Clear the reload value. */
    SysTick->VAL  = 0;                    /* Clear the current value. */
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;  /* Clear any pending SysTick interrupt. */
    HAL_DeInit();                         /* De-initialize all HAL peripherals. */
}


#if (USE_DFU_APP)
/* DFU trigger example: return true to jump into the DFU APP.
   The trigger source is entirely user-defined (GPIO level, external
   signal, communication state, ...). Example below: PA0 pulled low by
   a jumper / button / host controller. */
static bool 
stm32f1_CheckDFU( void )
{
    return ( HAL_GPIO_ReadPin( DFU_TRIG_GPIO_Port, DFU_TRIG_Pin ) == GPIO_PIN_RESET );
}
#endif /* USE_DFU_APP */


#if (USE_DG)
	static void 
	stm32f1_FeedIWDG( void )
	{
		uint16_t Reload = 0xAAAAUL;
		IWDG->KR = (Reload & 0xFFFFUL);
	}
#endif /* USE_DG */


static void 
Cus_Bootloader_Utils_SystemClockConfig( void )
{
	RCC_OscInitTypeDef OscInitStructure = { 0 };
	RCC_ClkInitTypeDef ClkInitStructure = { 0 };

	OscInitStructure.HSEState = RCC_HSE_ON;
	OscInitStructure.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	OscInitStructure.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	OscInitStructure.PLL.PLLState = RCC_PLL_ON;
	OscInitStructure.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	OscInitStructure.PLL.PLLMUL = RCC_PLL_MUL9;

	if ( HAL_RCC_OscConfig(&OscInitStructure) != HAL_OK )
	{
		err_handle();
	}

	ClkInitStructure.ClockType = RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK;
	ClkInitStructure.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	ClkInitStructure.AHBCLKDivider = RCC_SYSCLK_DIV1;
	ClkInitStructure.APB1CLKDivider = RCC_HCLK_DIV2;
	ClkInitStructure.APB2CLKDivider = RCC_HCLK_DIV1;

	if ( HAL_RCC_ClockConfig(&ClkInitStructure, FLASH_LATENCY_2) != HAL_OK )
	{
		err_handle();
	}
}


void 
BootPlatform_stm32f1_Install( void )
{
    /* Zero-initialize the whole ops table first: any member left
       unassigned (e.g. LogOut / DelayMs until the user provides an
       implementation) stays NULL instead of stack garbage. */
    BootPlatform_Ops_t Ops = { 0 };
    Ops.Init 	= stm32f1_Init;
    Ops.PrepareJump = stm32f1_PrepareJump;

	#if (USE_DFU_APP)
		Ops.CheckDFU = stm32f1_CheckDFU;
	#endif /* USE_DFU_APP */

	#if (USE_DG)
		Ops.FeedDg = stm32f1_FeedIWDG;
	#endif /* USE_DG */

    BootPlatform_Register(&Ops);
}




