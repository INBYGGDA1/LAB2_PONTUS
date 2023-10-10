/*
 * ================================================================
 * File: Lab2_2.1.c
 * Author: Pontus Svensson
 * Date: 2023-09-11
 * Description: Control the LED on the BoosterPack MKII using PWM and UART
 *
 * License: This code is distributed under the MIT License. visit
 * https://opensource.org/licenses/MIT for more information.
 * ================================================================
 */

/*================================================================*/
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/adc.h"

#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "inc/hw_memmap.h"
#include "utils/uartstdio.h"
#include "drivers/buttons.h"
#include "drivers/pinout.h"
#include "../drivers/tm4c129_functions.h"

#define PWM_LED GPIO_PIN_2

// on 100% The lowest is 10, where the led will turn off
// This is to prevent the pwm cycle to reset and start counting down from 100%

void pwm_width_calculator(volatile uint32_t *widthReturned,
                          volatile uint32_t brightness_controller) {
  // map values from 0-100 to (10 - 1000) as per lab description
  // normalize brightness between 0 and 1
  // float scale_brightness = (brightness_controller - 1) / (99.0 - 1);
  // *widthReturned = 10 + (pow(scale_brightness, 0.5) * (1000 - 10));
  *widthReturned = 10 + (brightness_controller * (1000 - 10) / 100);
}
//*****************************************************************************
//                      Main
//*****************************************************************************
int main(void) {

  // Set to 0 for assignment 1, and set to 1 for assignment2
  char buf[4];
  float pwm_word;
  unsigned char ucDelta, ucState;
  uint32_t brightness_controller = 50;
  volatile uint32_t systemClock = 0;
  volatile uint32_t old_brightness_value = 0;
  volatile uint32_t pulseWidth = 0;
  volatile uint32_t i = 0;
  volatile uint32_t k = 0;
  systemClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                    SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480),
                                   16000);

  // 10 000 000Hz/1000 = 10 000Hz
  // Prescaler = (System Clock) / (Timer Frequency)
  pwm_word = systemClock / 1000.0;
  PinoutSet(false, false);
  ButtonsInit();

  // Enable PWM
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  SysCtlPWMClockSet(SYSCTL_PWMDIV_1);
  SysCtlPeripheralDisable(SYSCTL_PERIPH_PWM0);
  SysCtlPeripheralReset(SYSCTL_PERIPH_PWM0);

  // 5 clk cycles should run before trying to use the PWM0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
  while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM0)) {
  }
  GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
  GPIOPinConfigure(GPIO_PF2_M0PWM2);

  // Sets the PWM mode, counts from 0 to a value, from a value down to zero or
  // from 0 to a value and then back down to 0
  // In this case we count down
  PWMGenConfigure(PWM0_BASE, PWM_GEN_1,
                  PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC |
                      PWM_GEN_MODE_DBG_RUN);

  // Specifies the period for PWM signal measured in clock ticks
  PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, pwm_word);

  //  duty cycle 50%
  //  PWMPulseWidthSet sets the pulse width for PWM_OUT_2, the width is defined
  //  as the number of PWM clock ticks
  pwm_width_calculator(&pulseWidth, 50);
  PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, pulseWidth);

  // PWMGenEnable enables the timer for the specified PWM generator block
  PWMGenEnable(PWM0_BASE, PWM_GEN_1);
  PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);

  ConfigureUART();

  while (1) {
    ucState = ButtonsPoll(&ucDelta, 0);

    // Only print over UART if the value has changed to reduce spamming
    if (old_brightness_value != brightness_controller) {
      UARTprintf("Brightness: %d%%\n", brightness_controller);
      old_brightness_value = brightness_controller;
    }
    UARTprintf("Enter LED percentage: ");
    UARTgets(buf, sizeof(buf));
    brightness_controller = atoi(buf);

    // For percentages between 0<x<100, use the pwm to set brightness
    if (brightness_controller > 0 && brightness_controller < 100) {
      // Make the PWM control the LED
      GPIOPinTypePWM(GPIO_PORTF_BASE, PWM_LED);
      pwm_width_calculator(&pulseWidth, brightness_controller);
      PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, pulseWidth);
    } else if (brightness_controller <= 0) {

      // Make the GPIO pin control the led
      GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, PWM_LED);
      GPIOPinWrite(GPIO_PORTF_BASE, PWM_LED, 0);
    } else if (brightness_controller >= 100) {

      // Make the GPIO pin control the led
      GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, PWM_LED);
      GPIOPinWrite(GPIO_PORTF_BASE, PWM_LED, PWM_LED);
    }
  }
  return 0;
}
