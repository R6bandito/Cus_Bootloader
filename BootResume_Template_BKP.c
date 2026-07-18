#include "BootResume_Template_BKP.h"



void Cus_Boot_RecordInit( void )
{
    uint32_t rcc_temp = RCC->APB1ENR;
    uint32_t pwr_temp = PWR->CR;
    if ( (rcc_temp & (0x01UL << 27)) != 1 || (rcc_temp & (0x01UL << 28)) != 1 || (pwr_temp & (0x01UL << 8)) != 1 )
    {
        // BKP域未开启访问. 将其开启.
        rcc_temp |= (0x01UL << 28);
        rcc_temp |= (0x01UL << 27);
        RCC->APB1ENR = rcc_temp;

        pwr_temp |= (0x01UL << 8);
        PWR->CR = pwr_temp;
    }
}


void Cus_Boot_Clearcb( void )
{
    *(volatile uint16_t *)RESUME_BKP_MAGIC_ADDR      = 0x00;
    *(volatile uint16_t *)RESUME_BKP_STATE_ADDR      = 0x00;
    *(volatile uint16_t *)RESUME_BKP_PACK_ADDR       = 0x00;
}


bool Cus_Boot_Savecb( const BootResume_Data_t *data )
{
    if ( *(volatile uint16_t *)RESUME_BKP_MAGIC_ADDR != PWRFAIL_RESUME_BKP_MAGIC )   
    {
        // 第一次写入时再写入魔数.
        *(volatile uint16_t *)RESUME_BKP_MAGIC_ADDR = PWRFAIL_RESUME_BKP_MAGIC;  // 魔数写入.
    }

    *(volatile uint16_t *)RESUME_BKP_PACK_ADDR = data->packs;
    __DSB();

    *(volatile uint16_t *)RESUME_BKP_STATE_ADDR = data->state;
    __DSB();

    return true;
}


bool Cus_Boot_Loadcb( BootResume_Data_t *data )
{
    if ( *(volatile uint16_t *)RESUME_BKP_MAGIC_ADDR != PWRFAIL_RESUME_BKP_MAGIC )    
        return false;   // 魔数验证失败,空调用.

    data->magic = *(volatile uint16_t *)RESUME_BKP_MAGIC_ADDR;
    data->packs = *(volatile uint16_t *)RESUME_BKP_PACK_ADDR;
    data->state = *(volatile uint16_t *)RESUME_BKP_STATE_ADDR;

    return true;
}


