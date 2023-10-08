/*
 * ================================================================
 * File: main.c
 * Author: Pontus Svensson
 * Date: 2023-10-07
 * Description:
 *
 * License: This code is distributed under the MIT License. visit
 * https://opensource.org/licenses/MIT for more information.
 * ================================================================
 */

/*================================================================*/
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
//=============================================================================
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/adc.h"
//=============================================================================
#include "inc/hw_memmap.h"
//=============================================================================
#include "CF128x128x16_ST7735S.h"
//=============================================================================
#include "grlib/grlib.h"
#include "tm4c129_functions.h"

//=============================================================================
#define MIC_SAMPLES 8
#define JOY_SAMPLES 4
#define ACC_SAMPLES 2
#define BUFFER_SIZE 50
#define PRINT_THRESHOLD 20
#define OPAQUE_TEXT true

int main(void) {
  tContext Context;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t joystick_x_samples[JOY_SAMPLES];
  uint32_t joystick_y_samples[JOY_SAMPLES];
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t joystick_x_average = 0;
  uint32_t joystick_y_average = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t samplesRead = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t systemClock =
      SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL |
                          SYSCTL_CFG_VCO_480),
                         120000000);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  PERIPH_init(SYSCTL_PERIPH_ADC0);
  SENSOR_enable(JOYSTICK | BUTTON_UP | BUTTON_DOWN);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Function to initialize the LCD using the TiwaWare peripheral driver library
  CF128x128x16_ST7735SInit(systemClock);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Set the background to be white
  CF128x128x16_ST7735SClear(ClrWhite);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // All drawing operations take place in terms of a drawing contex. This
  // context describes the display to which the operation occurs, the color and
  // font to use, and the region of the display to which the operation should be
  // limited (the clipping region). A drawing context must be initialized with
  // GrContextInit() before it can be used to perform drawing operations.
  // "TivaWare Graphics Library for C Series User's Guide (Rev. E)" p.15
  GrContextInit(&Context, &g_sCF128x128x16_ST7735S);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Set the font size for the drawing
  GrContextFontSet(&Context, &g_sFontCm14);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // This function sets the background color to be used for drawing opertions in
  // the specified drawing context. This will change the box behind the
  // foreground drawing. p.44
  GrContextBackgroundSet(&Context, ClrWhite);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // This function sets the color to be used for drawing operation in the
  // specified drawing context. This will change the text color. p.45
  GrContextForegroundSet(&Context, ClrBlack);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  GrFlush(&Context);

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  ADC_newSequence(ADC0_BASE, 0, ADC_CTL_CH9, JOY_SAMPLES);
  while (1) {
  }
}
