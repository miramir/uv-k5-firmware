#ifdef ENABLE_FLASHLIGHT

#include "driver/gpio.h"
#include "bsp/dp32g030/gpio.h"

#include "flashlight.h"

void ACTION_FlashLight(void)
{
    static bool gFlashLightState = false;

    if(gFlashLightState)
    {
        GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
    }
    else
    {
        GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);    
    }

    gFlashLightState = (gFlashLightState) ? false : true;
}
#endif
