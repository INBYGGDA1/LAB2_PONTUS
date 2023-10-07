/*
 * ================================================================
 * File: Lab2_4.2.c
 * Author: Pontus Svensson
 * Date: 2023-09-11
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
//=============================================================================
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/adc.h"
#include "driverlib/uart.h"
//=============================================================================
#include "utils/uartstdio.h"
//=============================================================================
#include "inc/hw_memmap.h"
//=============================================================================
#include "drivers/buttons.h"
#include "drivers/pinout.h"
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
// Configure the UART. Used for debugging purposes
//=============================================================================
void ConfigureUART(void) {
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  GPIOPinConfigure(GPIO_PA0_U0RX);
  GPIOPinConfigure(GPIO_PA1_U0TX);
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
  UARTStdioConfig(0, 115200, 16000000);
}

//=============================================================================
void ADC_newSequence(uint32_t ui32base, uint32_t ui32SequenceNum,
                     uint32_t ui32ADC_Channel, uint32_t ui32Samples) {
  int i;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Enable interrupts for the specified ADC_BASE
  // and sequence number
  ADCIntEnable(ui32base, ui32SequenceNum);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Clear any interrupts that might occur
  ADCIntClear(ui32base, ui32SequenceNum);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Disable the sequencer before making any changes
  ADCSequenceDisable(ui32base, ui32SequenceNum);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Set the trigger for the sampling sequence to be manual
  ADCSequenceConfigure(ui32base, ui32SequenceNum, ADC_TRIGGER_PROCESSOR, 0);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Configure each step in the sampling process. Here we could add set another
  // channel to sample from in another step.
  for (i = 0; i < ui32Samples; i++) {
    ADCSequenceStepConfigure(ui32base, ui32SequenceNum, i, ui32ADC_Channel);
  }
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // At the last step of the sequence, indicate that
  // it is the last and create an interrupt
  ADCSequenceStepConfigure(ui32base, ui32SequenceNum, ui32Samples - 1,
                           ADC_CTL_END | ADC_CTL_IE | ui32ADC_Channel);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Lastly we need to enable the sequence
  ADCSequenceEnable(ui32base, ui32SequenceNum);
}

//=============================================================================
// Helper function to sample the data
//=============================================================================
void sampleData(uint32_t ui32base, uint32_t ui32SequenceNum,
                uint32_t ui32ADC_Channel, uint32_t ui32Samples,
                uint32_t *ui32SamplesRead, void *buffer, uint32_t newSequence) {

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Should the sequence be reinitialized. If not we just trigger the sequence
  if (newSequence == 1) {
    ADC_newSequence(ui32base, ui32SequenceNum, ui32ADC_Channel, ui32Samples);
  }
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Trigger the sampling sequence
  ADCProcessorTrigger(ui32base, ui32SequenceNum);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Wait for the ADC to throw an interrupt indicating that the last sample has
  // been captured
  while (!ADCIntStatus(ui32base, ui32SequenceNum, false)) {
  }
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Place the ADC values into the buffer and return the number of samples
  // collected
  *ui32SamplesRead += ADCSequenceDataGet(ui32base, ui32SequenceNum, buffer);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Clear the interrupt status of the ADC to let other sequences sample their
  // sequences
  ADCIntClear(ui32base, ui32SequenceNum);
}

//=============================================================================
// Helper function to calculate the average of the collected ADC samples
//=============================================================================
void calculate_average(uint32_t ui32samples, uint32_t *buffer,
                       uint32_t *averageReturned) {
  uint32_t averageTemp = 0;
  int i = 0;
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Sum all the samples in the array
  for (i = 0; i < ui32samples; i++) {
    averageTemp += buffer[i];
  }
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Then I divide with number of samples in the array to get the average
  *averageReturned = averageTemp / ui32samples;
}

//=============================================================================
// Helper function to initialize the ADC
//=============================================================================
void ADC_init(void) {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Enable the ADC0 module
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)) {
  }
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Enable GPIO_PORTE Since this is where the
  // BoosterPack EDUMKII, joystick, accelerometer & microphone connects to.
  // See the table below.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE)) {
  }
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // ADC Pin Configuration
  //
  // Component    Axis     Pin    GPIO       Channel       AIN
  // --------------------------------------------------------
  // Microphone   N/A      PE5    GPIO_PIN_5  ADC_CTL_CH8  AIN8
  //
  // Joystick     X-Axis   PE4    GPIO_PIN_4  ADC_CTL_CH9  AIN9
  //              Y-Axis   PE3    GPIO_PIN_3  ADC_CTL_CH0  AIN0
  //
  // Gyroscope    X-Axis   PE0    GPIO_PIN_0  ADC_CTL_CH3  AIN3
  //              Y-Axis   PE1    GPIO_PIN_1  ADC_CTL_CH2  AIN2
  //              Z-Axis   PE2    GPIO_PIN_2  ADC_CTL_CH1  AIN1
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Enable all pins that should be set as ADC pins
  GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                                      GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
}

//=============================================================================
// Helper function to calculate absolute difference for uint32_t
//=============================================================================
uint32_t abs_diff(uint32_t a, uint32_t b) {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Since the variables are defined as unsigned integers, I cant subtract a - b
  // or vice versa since that could lead to a overflow issue. In this function i
  // check if a > b, then return a - b, else if b > a I return b - a. In order
  // to avoid taking the difference of a smaller unsigned integer with a greater
  // one.
  return (a > b) ? (a - b) : (b - a);
}

//=============================================================================
// Helper function to clear the text
//=============================================================================
void clear_text(tContext *sContext, int x, int y, int width, int height) {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // When updating the text on the screen, the values could have changed to be
  // << than the previous value and thus will not draw over the text furthest to
  // the right on the LCD. To circumvent this I draw a white rectangle across
  // the LCD screen with the height of the current fontSize.
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  tRectangle rect = {x, y, x + width, y + height};
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Since the background is white, I change the rectangle to white and fills
  // the are to avoid any overlap when drawing.
  GrContextForegroundSet(sContext, ClrWhite);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Draw the rectangle in the specified coordinates
  GrRectFill(sContext, &rect);
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Set the foreground back to black since I now am going to draw the text.
  GrContextForegroundSet(sContext, ClrBlack);
}

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
  ConfigureUART();
  UARTprintf("\033[2J");
  ADC_init();
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
