/*
 * ================================================================
 * File: Lab2_4.2.c
 * Author: Pontus Svensson
 * Date: 2023-10-07
 * Description: This program displays the values of the microphone,
 * accelerometer, and potentiometer (joystick) on the LCD screen using the
 * BoosterPack MKII. To run, the program has to be flashed twice on to the
 * TM4C129EXL for the proper output to appear!
 *
 * License: This code is distributed under the MIT License. visit
 * https://opensource.org/licenses/MIT for more information.
 * ================================================================
 */

/*================================================================*/
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
//=============================================================================
#include "tm4c129_functions.h"
//=============================================================================
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
//=============================================================================
#include "inc/hw_memmap.h"
//=============================================================================
#include "CF128x128x16_ST7735S.h"
//=============================================================================
#include "grlib/grlib.h"

//=============================================================================
#define MIC_SAMPLES 8
#define JOY_SAMPLES 4
#define ACC_SAMPLES 2
#define BUFFER_SIZE 50
#define PRINT_THRESHOLD 20
#define OPAQUE_TEXT true

//=============================================================================
// The error routine that is called if the driver library
// encounters an error.
//=============================================================================
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line) {
  while (1)
    ;
}
#endif

//=============================================================================
int main(void) {
  tContext sContext;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t microphone_samples[MIC_SAMPLES];
  uint32_t microphone_average = 0;
  uint32_t microphone_average_to_db = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // The previous values are used to compare if the values has changed
  // indicating user input. This should avoid spam printing the values.
  uint32_t microphone_previous = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t joystick_x_samples[JOY_SAMPLES];
  uint32_t joystick_y_samples[JOY_SAMPLES];
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t joystick_x_average = 0;
  uint32_t joystick_y_average = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t joystick_x_previous = 0;
  uint32_t joystick_y_previous = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t accelerometer_x_samples[ACC_SAMPLES];
  uint32_t accelerometer_y_samples[ACC_SAMPLES];
  uint32_t accelerometer_z_samples[ACC_SAMPLES];
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t accelerometer_x_average = 0;
  uint32_t accelerometer_y_average = 0;
  uint32_t accelerometer_z_average = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t accelerometer_x_previous = 0;
  uint32_t accelerometer_y_previous = 0;
  uint32_t accelerometer_z_previous = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  char microphone[BUFFER_SIZE];
  char joystick[BUFFER_SIZE];
  char accelerometer_x[BUFFER_SIZE];
  char accelerometer_y[BUFFER_SIZE];
  char accelerometer_z[BUFFER_SIZE];
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t samplesRead = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t toPrintOrNotToPrint = 0;
  uint32_t microphone_update = 0;
  uint32_t joystick_update = 0;
  uint32_t accelerometer_update = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  uint32_t systemClock =
      SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL |
                          SYSCTL_CFG_VCO_480),
                         120000000);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  ADC_init(SYSCTL_PERIPH_ADC0);
  GPIOPort_init(SYSCTL_PERIPH_GPIOE);
  SENSOR_enable(JOYSTICK | MICROPHONE | ACCELEROMETER);
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
  GrContextInit(&sContext, &g_sCF128x128x16_ST7735S);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Set the font size for the drawing
  GrContextFontSet(&sContext, &g_sFontCm14);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // This function sets the background color to be used for drawing opertions in
  // the specified drawing context. This will change the box behind the
  // foreground drawing. p.44
  GrContextBackgroundSet(&sContext, ClrWhite);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // This function sets the color to be used for drawing operation in the
  // specified drawing context. This will change the text color. p.45
  GrContextForegroundSet(&sContext, ClrBlack);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  GrFlush(&sContext);

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Set the sequence for sequence number 0, since it will only be utilized by
  // the microphone and will not need to be reinitialized.
  ADC_newSequence(ADC0_BASE, 0, ADC_CTL_CH8, MIC_SAMPLES);
  while (1) {
    samplesRead = 0;
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Microphone
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    sampleData(ADC0_BASE, 0, ADC_CTL_CH8, MIC_SAMPLES, &samplesRead,
               microphone_samples, 0);

    calculate_average(MIC_SAMPLES, microphone_samples, &microphone_average);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // I only want to update the values on the LCD if the change is big enough
    // to indicate that the user has done some input to the sensors
    if (abs_diff(microphone_average, microphone_previous) >= PRINT_THRESHOLD) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // Enable printing
      toPrintOrNotToPrint = 1;
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // Enable printing to update the microphone values
      microphone_update = 1;
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // Update the previous value
      microphone_previous = microphone_average;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Joystick-X
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    sampleData(ADC0_BASE, 1, ADC_CTL_CH9, JOY_SAMPLES, &samplesRead,
               joystick_x_samples, 1);
    calculate_average(JOY_SAMPLES, joystick_x_samples, &joystick_x_average);
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if (abs_diff(joystick_x_average, joystick_x_previous) >= PRINT_THRESHOLD) {
      toPrintOrNotToPrint = 1;
      joystick_update = 1;
      joystick_x_previous = joystick_x_average;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Joystick-Y
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    sampleData(ADC0_BASE, 1, ADC_CTL_CH0, JOY_SAMPLES, &samplesRead,
               joystick_y_samples, 1);
    calculate_average(JOY_SAMPLES, joystick_y_samples, &joystick_y_average);
    if (abs_diff(joystick_y_average, joystick_y_previous) >= PRINT_THRESHOLD) {
      toPrintOrNotToPrint = 1;
      joystick_update = 1;
      joystick_y_previous = joystick_y_average;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Accelerometer-X
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    sampleData(ADC0_BASE, 2, ADC_CTL_CH3, ACC_SAMPLES, &samplesRead,
               accelerometer_x_samples, 1);
    calculate_average(ACC_SAMPLES, accelerometer_x_samples,
                      &accelerometer_x_average);
    if (abs_diff(accelerometer_x_average, accelerometer_x_previous) >=
        PRINT_THRESHOLD) {
      toPrintOrNotToPrint = 1;
      accelerometer_update = 1;
      accelerometer_x_previous = accelerometer_x_average;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Accelerometer-Y
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    sampleData(ADC0_BASE, 2, ADC_CTL_CH2, ACC_SAMPLES, &samplesRead,
               accelerometer_y_samples, 1);
    calculate_average(ACC_SAMPLES, accelerometer_y_samples,
                      &accelerometer_y_average);
    if (abs_diff(accelerometer_y_average, accelerometer_y_previous) >=
        PRINT_THRESHOLD) {
      toPrintOrNotToPrint = 1;
      accelerometer_update = 1;
      accelerometer_y_previous = accelerometer_y_average;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Accelerometer-Z
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    sampleData(ADC0_BASE, 2, ADC_CTL_CH1, ACC_SAMPLES, &samplesRead,
               accelerometer_z_samples, 1);
    calculate_average(ACC_SAMPLES, accelerometer_z_samples,
                      &accelerometer_z_average);
    if (abs_diff(accelerometer_z_average, accelerometer_z_previous) >=
        PRINT_THRESHOLD) {
      toPrintOrNotToPrint = 1;
      accelerometer_update = 1;
      accelerometer_z_previous = accelerometer_z_average;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // If the values has changed indicating user input, I update the LCD on
    // screen values
    if (toPrintOrNotToPrint == 1) {

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      if (microphone_update == 1) {
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // Convert the microphone value to dB
        microphone_average_to_db = round(20.0 * log10(microphone_average));
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // sprintf(buf, max, "%s\n"%s)  is used to place the average value
        // in a string in order to be able to write it on the LCD
        snprintf(microphone, sizeof(microphone), "Mic: %u dB",
                 microphone_average_to_db);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // GrDefaultStringRenderer is the function used to draw the string on
        // the LCD, where I can specify the position of the text and the opacity
        // of the text
        GrDefaultStringRenderer(&sContext, microphone, strlen(microphone), 1,
                                20, OPAQUE_TEXT);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        microphone_update = 0;
      }
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      if (joystick_update == 1) {
        snprintf(joystick, sizeof(joystick), "Joy: %u-X, %u-Y",
                 joystick_x_average, joystick_y_average);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        clear_text(&sContext, 1, 40, 128, 14);
        GrDefaultStringRenderer(&sContext, joystick, strlen(joystick), 1, 40,
                                OPAQUE_TEXT);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        joystick_update = 0;
      }
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      if (accelerometer_update == 1) {
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // To write all axis on the screen in one line, the font size has to be
        // really small which I did not like
        snprintf(accelerometer_x, sizeof(accelerometer_x), "Acc: %u-X",
                 accelerometer_x_average);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        snprintf(accelerometer_y, sizeof(accelerometer_y), "Acc: %u-Y",
                 accelerometer_y_average);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        snprintf(accelerometer_z, sizeof(accelerometer_z), "Acc: %u-Z",
                 accelerometer_z_average);

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        GrDefaultStringRenderer(&sContext, accelerometer_x,
                                strlen(accelerometer_x), 1, 60, OPAQUE_TEXT);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        GrDefaultStringRenderer(&sContext, accelerometer_y,
                                strlen(accelerometer_y), 1, 80, OPAQUE_TEXT);
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        GrDefaultStringRenderer(&sContext, accelerometer_z,
                                strlen(accelerometer_z), 1, 100, OPAQUE_TEXT);
      }
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // Redraw the LCD screen
      GrFlush(&sContext);
      toPrintOrNotToPrint = 0;
    }
  }
}
