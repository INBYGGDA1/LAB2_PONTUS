/*
 * ================================================================
 * File: Lab2_2.2.c
 * Author: Pontus Svensson
 * Date: 2023-09-11
 * Description: Control the LED on the BoosterPack MKII using the joystick
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

#include "utils/uartstdio.c"
#include "drivers/buttons.h"
#include "drivers/pinout.h"

#define PWM_LED GPIO_PIN_2
#define SAMPLES 50

int moving_average(volatile uint32_t joystick_values[],
                   volatile uint32_t size) {
  float sum = 0;
  uint32_t i = 0;
  for (i = 0; i < size; i++) {
    sum += joystick_values[i];
  }
  return round(sum / size);
}
//***********************************************************************
//                       Configurations
//***********************************************************************
// Configure the UART.
void ConfigureUART() {
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  GPIOPinConfigure(GPIO_PA0_U0RX);
  GPIOPinConfigure(GPIO_PA1_U0TX);
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
  UARTStdioConfig(0, 115200, 16000000);
}
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
  uint32_t assignment2 = 0;
  float pwm_word;
  unsigned char ucDelta, ucState;
  uint32_t brightness_controller = 50;
  volatile uint32_t systemClock = 0;
  volatile uint32_t old_brightness_value = 0;
  volatile uint32_t pulseWidth = 0;
  volatile uint32_t adc_value_arr[SAMPLES];
  volatile float adc_reference_voltage = 3.3;
  volatile uint32_t i = 0;
  volatile uint32_t k = 0;

  // Reset the array
  for (i = 0; i < SAMPLES; i++) {
    adc_value_arr[i] = 0;
  }

  systemClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                    SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480),
                                   16000);

  // 10 000 000Hz/1000 = 10 000Hz
  // Prescaler = (System Clock) / (Timer Frequency)
  pwm_word = systemClock / 1000.0;

  // Make the LEDs controlled by the user software
  PinoutSet(false, false);
  ButtonsInit();

  // Enable the ADC0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)) {
  }

  // Enable the first sample sequencer to capture the value of channel 0 when
  // the processor trigger occurs.
  ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
  ADCSequenceStepConfigure(ADC0_BASE, 0, 0,
                           ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH0);
  ADCSequenceEnable(ADC0_BASE, 0);

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

    // Read the value from the ADC.
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);
    while (k < SAMPLES) {
      // Start collecting values
      ADCProcessorTrigger(ADC0_BASE, 0);
      while (!ADCIntStatus(ADC0_BASE, 0, false)) {
      }
      // Read the values received
      ADCSequenceDataGet(ADC0_BASE, 0, &brightness_controller);

      // Save the values to calculate an average
      adc_value_arr[k] = brightness_controller;
      k++;
    }
    k = 0;
    // Convert the bits to be between 0 and 100
    // If we want voltage we would multiply with the adc_reference_voltage
    // = 3.3V
    brightness_controller =
        roundf((moving_average(adc_value_arr, SAMPLES) / 4095.0) * 100.0);

    // The joystick is really difficult to get exactly 100 on
    if (brightness_controller > 0 && brightness_controller < 100) {
      // Make pwm control the led
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
    } else {
      ;
    }
  }
  return 0;
}
