/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      ht_hw.h
 *
 *  DESCRIPTION
 *      This file defines the Health Thermometer hardware specific routines.
 *
 *****************************************************************************/

#ifndef __HT_HW_H__
#define __HT_HW_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <bluetooth.h>
#include <timer.h>



/*============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "user_config.h"




/*============================================================================*
 *  Public data type
 *============================================================================*/

/* data type for different type of buzzer beeps */
typedef enum
{
    /* No beeps */
    buzzer_beep_off = 0,

    /* Short beep */
    buzzer_beep_short,

    /* Long beep */
    buzzer_beep_long,

    /* Two short beeps */
    buzzer_beep_twice,

    /* Three short beeps */
    buzzer_beep_thrice

}buzzer_beep_type;

typedef struct
{

#ifdef ENABLE_BUZZER
    /* Buzzer timer id */
    timer_id                    buzzer_tid;

    /* Variable for storing beep type.*/
    buzzer_beep_type            beep_type;

    /* Variable for keeping track of beep counts. This variable will be 
     * initialized to 0 on beep start and will incremented at every beep 
     * sound
     */
    uint16                      beep_count;
#endif /* ENABLE_BUZZER */

    /* Timer for button press */
    timer_id                    button_press_tid;

}APP_HW_DATA_T;

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/

/* Blood pressure application hardware data instance */
extern APP_HW_DATA_T                   g_app_hw_data;

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function is called to initialise Health Thermometer hardware */
extern void HtInitHardware(void);

/* This function initialises Health Thermometer hardware data structure */
extern void HtHwDataInit(void);

/* This function handles PIO Changed event */
extern void HandlePIOChangedEvent(void *data);

/* This function is called to trigger beeps of different types 
 * 'buzzer_beep_type'
 */
extern void SoundBuzzer(buzzer_beep_type beep_type);

#endif /* __HT_HW_H__ */
