/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      ht_hw.c
 *
 *  DESCRIPTION
 *      This file defines the Health Thermometer hardware specific routines.
 *
 *****************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <pio.h>
#include <pio_ctrlr.h>
#include <timer.h>
#include "battery_service.h"
/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "ht_hw.h"
#include "health_thermometer.h"
#include "health_thermo_service.h"
#include "ht_gatt.h"
#include "app_gatt_db.h"
#include "app_gatt.h"
#include "user_config.h"


/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Setup PIOs
 *  PIO3    Buzzer
 *  PIO11   Button
 */

#define BUZZER_PIO              (14)
#define BUTTON_LEFT             (0)
#define BUTTON_RIGHT            (3)
#define BUTTON_FASTER           (4)
#define BUTTON_BRAKE            (9)
#define BUTTON_GHG              (10)
#define BUTTON_BP               (11)

#define PIO_BIT_MASK(pio)       (0x01UL << (pio))

#define BUZZER_PIO_MASK         (PIO_BIT_MASK(BUZZER_PIO))

#define BUTTON_LEFT_MASK         (PIO_BIT_MASK(BUTTON_LEFT))
#define BUTTON_RIGHT_MASK        (PIO_BIT_MASK(BUTTON_RIGHT))
#define BUTTON_FASTER_MASK       (PIO_BIT_MASK(BUTTON_FASTER))
#define BUTTON_BRAKE_MASK        (PIO_BIT_MASK(BUTTON_BRAKE))
#define BUTTON_GHG_MASK          (PIO_BIT_MASK(BUTTON_GHG))
#define BUTTON_BP_MASK           (PIO_BIT_MASK(BUTTON_BP))

/* PIO direction */
#define PIO_DIRECTION_INPUT     (FALSE)
#define PIO_DIRECTION_OUTPUT    (TRUE)

/* Extra long button press timer */
#define EXTRA_LONG_BUTTON_PRESS_TIMER \
                                (4*SECOND)

#ifdef ENABLE_BUZZER

/* The index (0-3) of the PWM unit to be configured */
#define BUZZER_PWM_INDEX_0      (0)

/* PWM parameters for Buzzer */

/* Dull on. off and hold times ,以30us为基本单位*/
#define DULL_BUZZ_ON_TIME       (2)    /* 60us */
#define DULL_BUZZ_OFF_TIME      (15)   /* 450us */
#define DULL_BUZZ_HOLD_TIME     (0)

/* Bright on, off and hold times */
#define BRIGHT_BUZZ_ON_TIME     (2)    /* 60us */
#define BRIGHT_BUZZ_OFF_TIME    (15)   /* 450us */
#define BRIGHT_BUZZ_HOLD_TIME   (0)    /* 0us */

#define BUZZ_RAMP_RATE          (0xFF)

#endif /* ENABLE_BUZZER */
                                        
/* Enumeration to register the known button states */
typedef enum _button_state
{
    button_state_down,       /* Button was pressed */
    button_state_up,         /* Button was released */
    button_state_unknown     /* Button state is unknown */
} BUTTON_STATE_T;     
/*============================================================================*
 *  Public data
 *============================================================================*/

/* Blood pressure application hardware data instance */
APP_HW_DATA_T                   g_app_hw_data;

static BUTTON_STATE_T ghg_state = button_state_unknown;
static BUTTON_STATE_T bp_state = button_state_unknown;

uint8 ghg_count=0;
uint8 bp_count=0;


/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

#ifdef ENABLE_BUZZER

/*----------------------------------------------------------------------------*
 *  NAME
 *      appBuzzerTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to stop the Buzzer at the expiry of 
 *      timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
static void appBuzzerTimerHandler(timer_id tid)
{
    uint32 beep_timer = SHORT_BEEP_TIMER_VALUE;

    g_app_hw_data.buzzer_tid = TIMER_INVALID;

    switch(g_app_hw_data.beep_type)
    {
        case buzzer_beep_short: /* FALLTHROUGH */
        case buzzer_beep_long:
        {
            g_app_hw_data.beep_type = buzzer_beep_off;

            /* Disable buzzer */
            PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);
        }
        break;
        case buzzer_beep_twice:
        {
            if(g_app_hw_data.beep_count == 0)
            {
                /* First beep sounded. Start the silent gap*/
                g_app_hw_data.beep_count = 1;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                /* Time gap between two beeps */
                beep_timer = BEEP_GAP_TIMER_VALUE;
            }
            else if(g_app_hw_data.beep_count == 1)
            {
                g_app_hw_data.beep_count = 2;

                /* Enable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, TRUE);

                /* Start another short beep */
                beep_timer = SHORT_BEEP_TIMER_VALUE;
            }
            else
            {
                /* Two beeps have been sounded. Stop buzzer now*/
                g_app_hw_data.beep_count = 0;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                g_app_hw_data.beep_type = buzzer_beep_off;
            }
        }
        break;
        case buzzer_beep_thrice:
        {
            if(g_app_hw_data.beep_count == 0 ||
               g_app_hw_data.beep_count == 2)
            {
                /* First beep sounded. Start the silent gap*/
                ++ g_app_hw_data.beep_count;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                /* Time gap between two beeps */
                beep_timer = BEEP_GAP_TIMER_VALUE;
            }
            else if(g_app_hw_data.beep_count == 1 ||
                    g_app_hw_data.beep_count == 3)
            {
                ++ g_app_hw_data.beep_count;

                /* Enable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, TRUE);

                beep_timer = SHORT_BEEP_TIMER_VALUE;
            }
            else
            {
                /* Two beeps have been sounded. Stop buzzer now*/
                g_app_hw_data.beep_count = 0;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                g_app_hw_data.beep_type = buzzer_beep_off;
            }
        }
        break;

        default:
        {
            /* No such beep type */
            ReportPanic(app_panic_unexpected_beep_type);
            /* Though break statement will not be executed after panic but this
             * has been kept to avoid any confusion for default case.
             */
        }
        break;
    }

    if(g_app_hw_data.beep_type != buzzer_beep_off)
    {
        /* start the timer */
        g_app_hw_data.buzzer_tid = TimerCreate(beep_timer, TRUE, 
                                               appBuzzerTimerHandler);
    }
}

#endif /* ENABLE_BUZZER*/


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      HtInitHardware
 *
 *  DESCRIPTION
 *      This function is called to initialise Health Thermometer hardware
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HtInitHardware(void)
{
    /* Setup PIOs
     * PIO3  - Buzzer
     * PIO11 - Button
     */
    
    PioSetModes(BUTTON_LEFT_MASK, pio_mode_user);
    PioSetDir(BUTTON_LEFT, PIO_DIRECTION_INPUT);
    PioSetPullModes(BUTTON_LEFT_MASK, pio_mode_strong_pull_up);
    /* Setup button on PIO2 */
    PioSetEventMask(BUTTON_LEFT_MASK, pio_event_mode_both);
    
    PioSetModes(BUTTON_RIGHT_MASK, pio_mode_user);
    PioSetDir(BUTTON_RIGHT, PIO_DIRECTION_INPUT);
    PioSetPullModes(BUTTON_RIGHT_MASK, pio_mode_strong_pull_up);
    /* Setup button on PIO3 */
    PioSetEventMask(BUTTON_RIGHT_MASK, pio_event_mode_both);
  
    PioSetModes(BUTTON_FASTER_MASK, pio_mode_user);
    PioSetDir(BUTTON_FASTER, PIO_DIRECTION_INPUT);
    PioSetPullModes(BUTTON_FASTER_MASK, pio_mode_strong_pull_up);
    /* Setup button on PIO4 */
    PioSetEventMask(BUTTON_FASTER_MASK, pio_event_mode_both);
    
    PioSetModes(BUTTON_BRAKE_MASK, pio_mode_user);
    PioSetDir(BUTTON_BRAKE, PIO_DIRECTION_INPUT);
    PioSetPullModes(BUTTON_BRAKE_MASK, pio_mode_strong_pull_up);
    /* Setup button on PIO9 */
    PioSetEventMask(BUTTON_BRAKE_MASK, pio_event_mode_both);    
    
    PioSetModes(BUTTON_GHG_MASK, pio_mode_user);
    PioSetDir(BUTTON_GHG, PIO_DIRECTION_INPUT);
    PioSetPullModes(BUTTON_GHG_MASK, pio_mode_strong_pull_up);
    /* Setup button on PIO10 */
    PioSetEventMask(BUTTON_GHG_MASK, pio_event_mode_both);    

    PioSetModes(BUTTON_BP_MASK, pio_mode_user);
    PioSetDir(BUTTON_BP, PIO_DIRECTION_INPUT);
    PioSetPullModes(BUTTON_BP_MASK, pio_mode_strong_pull_up);
    /* Setup button on PIO11 */
    PioSetEventMask(BUTTON_BP_MASK, pio_event_mode_both);      
#ifdef ENABLE_BUZZER
    PioSetModes(BUZZER_PIO_MASK, pio_mode_pwm0);

    /* Configure the buzzer on PIO3 */
    PioConfigPWM(BUZZER_PWM_INDEX_0, pio_pwm_mode_push_pull, DULL_BUZZ_ON_TIME,
                 DULL_BUZZ_OFF_TIME, DULL_BUZZ_HOLD_TIME, BRIGHT_BUZZ_ON_TIME,
                 BRIGHT_BUZZ_OFF_TIME, BRIGHT_BUZZ_HOLD_TIME, BUZZ_RAMP_RATE);


    PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);
#endif /* ENABLE_BUZZER */

    /* Save power by changing the I2C pull mode to pull down */
    PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HtHwDataInit
 *
 *  DESCRIPTION
 *      This function initialises Health Thermometer hardware data structure
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HtHwDataInit(void)
{

    /* Delete button press timer */
    TimerDelete(g_app_hw_data.button_press_tid);
    g_app_hw_data.button_press_tid = TIMER_INVALID;

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      SoundBuzzer
 *
 *  DESCRIPTION
 *      This function is called to trigger beeps of different types 
 *      'buzzer_beep_type'.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void SoundBuzzer(buzzer_beep_type beep_type)
{
#ifdef ENABLE_BUZZER
    uint32 beep_timer = SHORT_BEEP_TIMER_VALUE;

    PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

    TimerDelete(g_app_hw_data.buzzer_tid);
    g_app_hw_data.buzzer_tid = TIMER_INVALID;

    g_app_hw_data.beep_count = 0;

    /* Store the beep type in some global variable. It will used on timer 
     * expiry to check the type of beeps being sounded.
     */
    g_app_hw_data.beep_type = beep_type;

    switch(g_app_hw_data.beep_type)
    {
        case buzzer_beep_off:
        {
            /* Don't do anything */
        }
        break;

        case buzzer_beep_short: /* One short beep will be sounded */
            /* FALLTHROUGH */
        case buzzer_beep_twice: /* Two short beeps will be sounded */
            /* FALLTHROUGH */
        case buzzer_beep_thrice: /* Three short beeps will be sounded */
        {
            beep_timer = SHORT_BEEP_TIMER_VALUE;
        }
        break;

        case buzzer_beep_long:
        {
            /* One long beep will be sounded */
            beep_timer = LONG_BEEP_TIMER_VALUE;
        }
        break;

        default:
        {
            /* No such beep type defined */
            ReportPanic(app_panic_unexpected_beep_type);
            /* Though break statement will not be executed after panic but this
             * has been kept to avoid any confusion for default case.
             */
        }
        break;
    }

    if(g_app_hw_data.beep_type != buzzer_beep_off)
    {
        /* Initialize beep count to zero */
        g_app_hw_data.beep_count = 0;

        /* Enable buzzer，The timeout period is measured in microseconds. time.h defines a number of constants
 for MILLISECOND, SECOND and MINUTE, e.g. allowing 10*SECOND to be used when starting a timer.
 Note that although the timeout value is a 32-bit number the maximum timeout period is actually (2^31)-1 microseconds 
(not (2^32)-1 microseconds) to enable safe 'roll over' handling. (2^31)-1 microseconds corresponds to approximately
 35 minutes 47 seconds. */
        PioEnablePWM(BUZZER_PWM_INDEX_0, TRUE);

        TimerDelete(g_app_hw_data.buzzer_tid);
        g_app_hw_data.buzzer_tid = TimerCreate(beep_timer, TRUE, 
                                               appBuzzerTimerHandler);
    }
#endif /* ENABLE_BUZZER */
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      HandlePIOChangedEvent
 *
 *  DESCRIPTION
 *      This function handles PIO Changed event
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandlePIOChangedEvent(void *data)
{
    const pio_changed_data *pPioData = (const pio_changed_data *)data;    
    bool valid_ghgStateChange = FALSE;
    bool valid_bpStateChange = FALSE;
    uint8  val[5];
    val[4]=readBatteryLevel();
    uint8 switchs=0xFF;
    
    if(pPioData->pio_cause & BUTTON_LEFT_MASK)
    {
       
        if(!(pPioData->pio_state & BUTTON_LEFT_MASK))
        {
            /* This event comes when a button is pressed */

            /* Start a timer for LONG_BUTTON_PRESS_TIMER seconds. If timer expi-
             * res before we receive a button release event then it was a long -
             * press and if we receive a button release pio changed event, it -
             * means it was a short press.
             */
            TimerDelete(g_app_hw_data.button_press_tid);

            g_app_hw_data.button_press_tid = TimerCreate(
                                           EXTRA_LONG_BUTTON_PRESS_TIMER,
                                           TRUE,
                                           HandleExtraLongButtonPress);
             switchs &= 0xBF;/*(1011 1111)*/
        }
        else
        {
            /* This event comes when a button is released */
            if(g_app_hw_data.button_press_tid != TIMER_INVALID)
            {
                /* Timer was already running. This means it was a short button 
                 * press.
                 */
                TimerDelete(g_app_hw_data.button_press_tid);
                g_app_hw_data.button_press_tid = TIMER_INVALID;

              
                switchs &= 0x7F;/*(0111 1111)*/
            }
        }
    }

    if(pPioData->pio_cause & BUTTON_RIGHT_MASK)
    {
 
        if(pPioData->pio_state & BUTTON_RIGHT_MASK)
        {           
             switchs &= 0xDF;/*(1101 1111)*/
        }
        else            
        {
            switchs &= 0xEF;/*(1110 1111)*/                 
        }
    }
    if(pPioData->pio_cause & BUTTON_FASTER_MASK)
    {
 
        if(pPioData->pio_state & BUTTON_FASTER_MASK)
        {           
             switchs &= 0xF7;/*(1111 0111)*/
        }
        else            
        {
            switchs &= 0xFB;/*(1111 1011)*/                 
        }
    }      
    if(pPioData->pio_cause & BUTTON_BRAKE_MASK)
    {
 
        if(pPioData->pio_state & BUTTON_BRAKE_MASK)
        {           
             switchs &= 0xFD;/*(1111 1101)*/
        }
        else            
        {
            switchs &= 0xFE;/*(1111 1110)*/                 
        }
    } 
    if(pPioData->pio_cause & BUTTON_GHG_MASK)
    {
        valid_ghgStateChange = FALSE;
        if(pPioData->pio_state & BUTTON_GHG_MASK)
        {           
            ghg_state= button_state_up;
        }
        else            
        {
            if (ghg_state == button_state_up)
            {
                valid_ghgStateChange = TRUE;
            }
            ghg_state= button_state_down;                 
        }
         if (valid_ghgStateChange)
        {
            if(ghg_count==0xFF)
            {
                ghg_count=0;
            }
            else
            {
                ghg_count+=1;
            }
        }
    }         
    if(pPioData->pio_cause & BUTTON_BP_MASK)
    {
        valid_bpStateChange = FALSE;
        if(pPioData->pio_state & BUTTON_BP_MASK)
        {           
            bp_state= button_state_up;
        }
        else            
        {
            if (bp_state == button_state_up)
            {
                valid_bpStateChange = TRUE;
            }
            bp_state= button_state_down;                 
        }
         if (valid_bpStateChange)
        {
            if(bp_count==0xFF)
            {
                bp_count=0;
            }
            else
            {
                bp_count+=1;
            }
        }
    }         

    val[3]=switchs;
    val[2]=ghg_count;
    val[1]=bp_count;

    HandleShortButtonPress(val);
   
}


