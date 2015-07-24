/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      health_thermometer.c
 *
 *  DESCRIPTION
 *      This file defines a simple implementation of Health Thermometer service
 *
 *****************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <main.h>
#include <types.h>
#include <timer.h>
#include <mem.h>

/* Upper Stack API */
#include <gatt.h>
#include <gatt_prim.h>
#include <ls_app_if.h>
#include <gap_app_if.h>
#include <buf_utils.h>
#include <security.h>
#include <panic.h>
#include <nvm.h>
#include <thermometer.h>

/*============================================================================*
 *  Local Header File
 *============================================================================*/

#include "app_gatt.h"
#include "app_gatt_db.h"
#include "nvm_access.h"
#include "health_thermometer.h"
#include "ht_gatt.h"
#include "ht_hw.h"
#include "gap_service.h"
#include "health_thermo_service.h"
#include "battery_service.h"

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Maximum number of timers */
#define MAX_APP_TIMERS                 (5)

/*Number of IRKs that application can store */
#define MAX_NUMBER_IRK_STORED          (1)

/* Magic value to check the sanity of NVM region used by the application */
#define NVM_SANITY_MAGIC               (0xAB08)

/* NVM offset for NVM sanity word */
#define NVM_OFFSET_SANITY_WORD         (0)

/* NVM offset for bonded flag */
#define NVM_OFFSET_BONDED_FLAG         (NVM_OFFSET_SANITY_WORD + 1)

/* NVM offset for bonded device bluetooth address */
#define NVM_OFFSET_BONDED_ADDR         (NVM_OFFSET_BONDED_FLAG + \
                                        sizeof(g_ht_data.bonded))

/* NVM offset for diversifier */
#define NVM_OFFSET_SM_DIV              (NVM_OFFSET_BONDED_ADDR + \
                                        sizeof(g_ht_data.bonded_bd_addr))

/* NVM offset for IRK */
#define NVM_OFFSET_SM_IRK              (NVM_OFFSET_SM_DIV + \
                                        sizeof(g_ht_data.diversifier))

/* Number of words of NVM used by application. Memory used by supported 
 * services is not taken into consideration here.
 */
#define NVM_MAX_APP_MEMORY_WORDS       (NVM_OFFSET_SM_IRK + \
                                        MAX_WORDS_IRK)

/* Slave device is not allowed to transmit another Connection Parameter 
 * Update request till time TGAP(conn_param_timeout). Refer to section 9.3.9.2,
 * Vol 3, Part C of the Core 4.0 BT spec. The application should retry the 
 * 'Connection Parameter Update' procedure after time TGAP(conn_param_timeout)
 * which is 30 seconds.
 */
#define GAP_CONN_PARAM_TIMEOUT         (30 * SECOND)

/* TGAP(conn_pause_peripheral) defined in Core Specification Addendum 3 Revision
 * 2. A Peripheral device should not perform a Connection Parameter Update proc-
 * -edure within TGAP(conn_pause_peripheral) after establishing a connection.
 */
#define TGAP_CPP_PERIOD                (5 * SECOND)

/* TGAP(conn_pause_central) defined in Core Specification Addendum 3 Revision 2.
 * After the Peripheral device has no further pending actions to perform and the
 * Central device has not initiated any other actions within TGAP(conn_pause_ce-
 * -ntral), then the Peripheral device may perform a Connection Parameter Update
 * procedure.
 */
#define TGAP_CPC_PERIOD                (1 * SECOND)


/* Time after which measured tempeature will be transmitted 
 * to the connected host.
 */
#define HT_TEMP_MEAS_TIME              (40 * SECOND)

/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Declare space for application timers. */
static uint16 app_timers[SIZEOF_APP_TIMER * MAX_APP_TIMERS];
                                   


/*============================================================================*
 *  Public Data
 *============================================================================*/

/* Health Thermometer application data instance */
HT_DATA_T g_ht_data;

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void htDataInit(void);
static void readPersistentStore(void);
static void requestConnParamUpdate(timer_id tid);
static void htTempMeasTimerHandler(timer_id tid);
static void appInitExit(void);
static void appAdvertisingExit(void);
static void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *p_event_data);
static void handleBondingChanceTimerExpiry(timer_id tid);
static void handleSignalLmDisconnectComplete(HCI_EV_DATA_DISCONNECT_COMPLETE_T 
                                                                *p_event_data);
static void handleSignalLmConnectionUpdate(
                                       LM_EV_CONNECTION_UPDATE_T* p_event_data);
static void handleGapCppTimerExpiry(timer_id tid);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      htDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise health thermometer application data 
 *      structure.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void htDataInit(void)
{

    TimerDelete(g_ht_data.app_tid);
    g_ht_data.app_tid = TIMER_INVALID;

    TimerDelete(g_ht_data.con_param_update_tid);
    g_ht_data.con_param_update_tid = TIMER_INVALID;
    g_ht_data.cpu_timer_value = 0;

    /* Delete the bonding chance timer */
    TimerDelete(g_ht_data.bonding_reattempt_tid);
    g_ht_data.bonding_reattempt_tid = TIMER_INVALID;
    

    g_ht_data.st_ucid = GATT_INVALID_UCID;

    g_ht_data.encrypt_enabled = FALSE;

    /* Reset the connection parameter variables. */
    g_ht_data.conn_interval = 0;
    g_ht_data.conn_latency = 0;
    g_ht_data.conn_timeout = 0;

    /* Health thermometer hardware data initialisation */
    HtHwDataInit();

    /* Initialise GAP Data structure */
    GapDataInit();

    /* Battery Service data initialisation */
    BatteryDataInit();

    /* Health Thermometer Service data initialisation */
    HealthThermoDataInit();

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      readPersistentStore
 *
 *  DESCRIPTION
 *      This function is used to initialise and read NVM data
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void readPersistentStore(void)
{
    /* NVM offset for supported services */
    uint16 nvm_offset = NVM_MAX_APP_MEMORY_WORDS;
    uint16 nvm_sanity = 0xffff;

    /* Read persistent storage to know if the device was last bonded 
     * to another device 
     */

    /* If the device was bonded, trigger fast undirected advertisements by 
     * setting the white list for bonded host. If the device was not bonded,
     * trigger undirected advertisements for any host to connect.
     */

    Nvm_Read(&nvm_sanity, 
             sizeof(nvm_sanity), 
             NVM_OFFSET_SANITY_WORD);

    if(nvm_sanity == NVM_SANITY_MAGIC)
    {

        /* Read Bonded Flag from NVM */
        Nvm_Read((uint16*)&g_ht_data.bonded,
                  sizeof(g_ht_data.bonded),
                  NVM_OFFSET_BONDED_FLAG);

        if(g_ht_data.bonded)
        {
            /* Bonded Host Typed BD Address will only be stored if bonded flag
             * is set to TRUE. Read last bonded device address.
             */
            Nvm_Read((uint16*)&g_ht_data.bonded_bd_addr, 
                       sizeof(TYPED_BD_ADDR_T),
                       NVM_OFFSET_BONDED_ADDR);

            /* If device is bonded and bonded address is resolvable then read 
             * the bonded device's IRK
             */
            if(GattIsAddressResolvableRandom(&g_ht_data.bonded_bd_addr))
            {
                Nvm_Read(g_ht_data.central_device_irk.irk, 
                         MAX_WORDS_IRK,
                         NVM_OFFSET_SM_IRK);
            }

        }
        else /* Case when we have only written NVM_SANITY_MAGIC to NVM but 
              * didn't get bonded to any host in the last powered session
              */
        {
            g_ht_data.bonded = FALSE;
        }

        /* Read the diversifier associated with the presently bonded/last 
         * bonded device.
         */
        Nvm_Read(&g_ht_data.diversifier, 
                 sizeof(g_ht_data.diversifier),
                 NVM_OFFSET_SM_DIV);

        /* If NVM in use, read device name and length from NVM */
        GapReadDataFromNVM(&nvm_offset);

    }
    else /* NVM Sanity check failed means either the device is being brought up 
          * for the first time or memory has got corrupted in which case 
          * discard the data and start fresh.
          */
    {

        nvm_sanity = NVM_SANITY_MAGIC;

        /* Write NVM Sanity word to the NVM */
        Nvm_Write(&nvm_sanity, 
                  sizeof(nvm_sanity), 
                  NVM_OFFSET_SANITY_WORD);

        /* The device will not be bonded as it is coming up for the first 
         * time 
         */
        g_ht_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&g_ht_data.bonded, 
                  sizeof(g_ht_data.bonded), 
                  NVM_OFFSET_BONDED_FLAG);

        /* When the application is coming up for the first time after flashing 
         * the image to it, it will not have bonded to any device. So, no LTK 
         * will be associated with it. Hence, set the diversifier to 0.
         */
        g_ht_data.diversifier = 0;

        /* Write the same to NVM. */
        Nvm_Write(&g_ht_data.diversifier, 
                  sizeof(g_ht_data.diversifier),
                  NVM_OFFSET_SM_DIV);

        /* If fresh NVM, write device name and length to NVM for the 
         * first time.
         */
        GapInitWriteDataToNVM(&nvm_offset);
    
    }

    /* Read Health Thermometer service data from NVM if the devices are bonded 
     * and update the offset with the number of word of NVM required by 
     * this service
     */
    HealthThermoReadDataFromNVM(&nvm_offset);

    /* Read Battery service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    BatteryReadDataFromNVM(&nvm_offset);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      requestConnParamUpdate
 *
 *  DESCRIPTION
 *      This function is used to send L2CAP_CONNECTION_PARAMETER_UPDATE_REQUEST 
 *      to the remote device when an earlier sent request had failed.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void requestConnParamUpdate(timer_id tid)
{
    /* Application specific preferred paramters */
     ble_con_params app_pref_conn_param;

    if(g_ht_data.con_param_update_tid == tid)
    {

        g_ht_data.con_param_update_tid = TIMER_INVALID;
        g_ht_data.cpu_timer_value = 0;

        /*Handling signal as per current state */
        switch(g_ht_data.state)
        {

            case app_state_connected:
            {
                /* Increment the count for connection parameter update 
                 * requests 
                 */
                ++ g_ht_data.num_conn_update_req;

                /* Decide which parameter values are to be requested.
                 */
                if(g_ht_data.num_conn_update_req <= 
                                                CPU_SELF_PARAMS_MAX_ATTEMPTS)
                {
                    app_pref_conn_param.con_max_interval = 
                                                PREFERRED_MAX_CON_INTERVAL;
                    app_pref_conn_param.con_min_interval = 
                                                PREFERRED_MIN_CON_INTERVAL;
                    app_pref_conn_param.con_slave_latency = 
                                                PREFERRED_SLAVE_LATENCY;
                    app_pref_conn_param.con_super_timeout = 
                                                PREFERRED_SUPERVISION_TIMEOUT;
                }
                else
                {
                    app_pref_conn_param.con_max_interval = 
                                                APPLE_MAX_CON_INTERVAL;
                    app_pref_conn_param.con_min_interval = 
                                                APPLE_MIN_CON_INTERVAL;
                    app_pref_conn_param.con_slave_latency = 
                                                APPLE_SLAVE_LATENCY;
                    app_pref_conn_param.con_super_timeout = 
                                                APPLE_SUPERVISION_TIMEOUT;
                }

                /* Send Connection Parameter Update request using application 
                 * specific preferred connection parameters
                 */

                if(LsConnectionParamUpdateReq(&g_ht_data.con_bd_addr, 
                                &app_pref_conn_param) != ls_err_none)
                {
                    ReportPanic(app_panic_con_param_update);
                }


            }
            break;

            default:
                /* Ignore in other states */
            break;
        }

    } /* Else ignore the timer */

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      htTempMeasTimerHandler
 *
 *  DESCRIPTION
 *      This function is called repeatedly via a timer to transmit 
 *      temperature measurements
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void htTempMeasTimerHandler(timer_id tid)
{
    /*
    (modification)
     int16 new_temp_r;
     */
    if(tid == g_ht_data.app_tid)
    {
        g_ht_data.app_tid = TIMER_INVALID;

        /*Handling signal as per current state */
        switch(g_ht_data.state)
        {
            case app_state_connected:
            {

                /* Send thermometer measurement only if encryption is enabled */
                if(g_ht_data.encrypt_enabled)
                {
                    /* Read the temperature value 
                    (modification)
                    new_temp_r = ThermometerReadTemperature();
                    */

                    /* Send the updated value to the connected client */
                    
                    
                }

                /* Restart thermometer measurement timer */
                g_ht_data.app_tid = TimerCreate(HT_TEMP_MEAS_TIME,
                                                TRUE, htTempMeasTimerHandler);

            }
            break;

            default:
                /* Ignore the timer */
            break;
        }
    } /* Else ignore the timer */

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      appInitExit
 *
 *  DESCRIPTION
 *      This function is called upon exiting from app_state_init state. The 
 *      application starts advertising after exiting this state.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void appInitExit(void)
{
        if(g_ht_data.bonded && 
            (!GattIsAddressResolvableRandom(&g_ht_data.bonded_bd_addr)))
        {
            /* If the device is bonded and bonded device address is not
             * resolvable random, configure White list with the Bonded 
             * host address 
             */
            if(LsAddWhiteListDevice(&g_ht_data.bonded_bd_addr) !=
                ls_err_none)
            {
                ReportPanic(app_panic_add_whitelist);
            }
        }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      appAdvertisingExit
 *
 *  DESCRIPTION
 *      This function is called while exiting app_state_fast_advertising and
 *      app_state_slow_advertising states.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void appAdvertisingExit(void)
{
        /* Cancel advertisement timer */
        TimerDelete(g_ht_data.app_tid);
        g_ht_data.app_tid = TIMER_INVALID;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAddDBCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ADD_DB_CFM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattAddDbCfm(GATT_ADD_DB_CFM_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_init:
        {
            if(p_event_data->result == sys_status_success)
            {
                AppSetState(app_state_fast_advertising);
            }
            else
            {
                /* Don't expect this to happen */
                ReportPanic(app_panic_db_registration);
            }
        }
        break;

        default:
            /*Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattCancelConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CANCEL_CONNECT_CFM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattCancelConnectCfm(void)
{

    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_fast_advertising:
        case app_state_slow_advertising:
        {
            if(g_ht_data.pairing_button_pressed)
            {
                g_ht_data.pairing_button_pressed = FALSE;

                /* Reset and clear the whitelist */
                LsResetWhiteList();

                /* Trigger fast advertisements */
                if(g_ht_data.state == app_state_fast_advertising)
                {
                    GattStartAdverts(TRUE);
                }
                else
                {
                    AppSetState(app_state_fast_advertising);
                }
            }
            else
            {
                if(g_ht_data.state == app_state_fast_advertising)
                {
                    /* Trigger slow advertisements */
                    AppSetState(app_state_slow_advertising);
                }
                else
                {
                    /* Move to app_state_idle state */
                    AppSetState(app_state_idle);
                }
            }
        }
        break;
        
        default:
            /*Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }

}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      handleSignalLmEvConnectionComplete
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_COMPLETE.
 *
 *  RETURNS
 *      Nothing.
 *
 
*----------------------------------------------------------------------------*/
static void handleSignalLmEvConnectionComplete(
                                     LM_EV_CONNECTION_COMPLETE_T *p_event_data)
{
    /* Store the connection parameters. */
    g_ht_data.conn_interval = p_event_data->data.conn_interval;
    g_ht_data.conn_latency = p_event_data->data.conn_latency;
    g_ht_data.conn_timeout = p_event_data->data.supervision_timeout;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleGapCppTimerExpiry
 *
 *  DESCRIPTION
 *      This function handles the expiry of TGAP(conn_pause_peripheral) timer.
 *      It starts the TGAP(conn_pause_central) timer, during which, if no activ-
 *      -ity is detected from the central device, a connection parameter update
 *      request is sent.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleGapCppTimerExpiry(timer_id tid)
{
    if(g_ht_data.con_param_update_tid == tid)
    {
        g_ht_data.con_param_update_tid = 
                           TimerCreate(TGAP_CPC_PERIOD, TRUE,
                                       requestConnParamUpdate);
        g_ht_data.cpu_timer_value = TGAP_CPC_PERIOD;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CONNECT_CFM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattConnectCfm(GATT_CONNECT_CFM_T* p_event_data)
{
    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_fast_advertising:
        case app_state_slow_advertising:
        {
            if(p_event_data->result == sys_status_success)
            {
                /* Store received UCID */
                g_ht_data.st_ucid = p_event_data->cid;

                /* Store connected BD Address */
                g_ht_data.con_bd_addr = p_event_data->bd_addr;

                if(g_ht_data.bonded && 
                    GattIsAddressResolvableRandom(&g_ht_data.bonded_bd_addr) &&
                    (SMPrivacyMatchAddress(&p_event_data->bd_addr,
                                            g_ht_data.central_device_irk.irk,
                                            MAX_NUMBER_IRK_STORED, 
                                            MAX_WORDS_IRK) < 0))
                {
                    /* Application was bonded to a remote device using 
                     * resolvable random address and application has failed 
                     * to resolve the remote device address to which we just 
                     * connected, so disconnect and start advertising again
                     */

                    /* Disconnect if we are connected */
                    AppSetState(app_state_disconnecting);
                }
                else
                {
                    /* Enter connected state 
                     * - If the device is not bonded OR
                     * - If the device is bonded and the connected host doesn't 
                     *   support Resolvable Random address OR
                     * - If the device is bonded and connected host supports 
                     *   Resolvable Random address and the address gets resolved
                     *   using the store IRK key
                     */
                    AppSetState(app_state_connected);

                    /* If the current connection parameters being used don't 
                     * comply with the application's preferred connection 
                     * parameters and the timer is not running and , start timer
                     * to trigger Connection Parameter Update procedure
                     */
                    if((g_ht_data.con_param_update_tid == TIMER_INVALID) &&
                       (g_ht_data.conn_interval < PREFERRED_MIN_CON_INTERVAL ||
                        g_ht_data.conn_interval > PREFERRED_MAX_CON_INTERVAL
#if PREFERRED_SLAVE_LATENCY
                        || g_ht_data.conn_latency < PREFERRED_SLAVE_LATENCY
#endif
                       )
                      )
                    {
                        /* Set the num of conn update attempts to zero */
                        g_ht_data.num_conn_update_req = 0;

                        /* The application first starts a timer of 
                         * TGAP_CPP_PERIOD. During this time, the application 
                         * waits for the peer device to do the database 
                         * discovery procedure. After expiry of this timer, the 
                         * application starts one more timer of period 
                         * TGAP_CPC_PERIOD. If the application receives any 
                         * GATT_ACCESS_IND during this time, it assumes that 
                         * the peer device is still doing device database 
                         * discovery procedure or some other configuration and 
                         * it should not update the parameters, so it restarts 
                         * the TGAP_CPC_PERIOD timer. If this timer expires, the
                         * application assumes that database discovery procedure
                         * is complete and it initiates the connection parameter
                         * update procedure.
                         * Please note that this procedure requires all the 
                         * characteristic read/writes to be made IRQ. If 
                         * application wants firmware to reply for any of the 
                         * request, it shall reply with 
                         * "gatt_status_irq_proceed".
                         */
                        g_ht_data.con_param_update_tid = TimerCreate(
                                                TGAP_CPP_PERIOD,
                                                TRUE, handleGapCppTimerExpiry);
                        g_ht_data.cpu_timer_value = TGAP_CPP_PERIOD;

                    } /* Else at the expiry of timer Connection parameter 
                       * update procedure will get triggered
                       */
                }
            }
            else
            {
                /* Connection failure - Trigger fast advertisements */
                if(g_ht_data.state == app_state_slow_advertising)
                {
                    AppSetState(app_state_fast_advertising);
                }
                else
                {
                    /* Already in app_state_fast_advertising state, so just 
                     * trigger fast advertisements
                     */
                    GattStartAdverts(TRUE);
                }
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmKeysInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_KEYS_IND and copies IRK from it
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalSmKeysInd(SM_KEYS_IND_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
        {
            /* If keys are present, save them */
            if((p_event_data->keys)->keys_present & (1 << SM_KEY_TYPE_DIV))
            {
                /* Store the diversifier which will be used for accepting/
                 * rejecting the encryption requests.
                 */
                g_ht_data.diversifier = (p_event_data->keys)->div;

                /* Write the new diversifier to NVM */
                Nvm_Write(&g_ht_data.diversifier,
                          sizeof(g_ht_data.diversifier), 
                          NVM_OFFSET_SM_DIV);
            }

            /* Store IRK if the connected host is using random resolvable 
             * address. IRK is used afterwards to validate the identity of 
             * connected host 
             */
            if(GattIsAddressResolvableRandom(&g_ht_data.con_bd_addr) &&
               ((p_event_data->keys)->keys_present & (1 << SM_KEY_TYPE_ID)))
            {
                MemCopy(g_ht_data.central_device_irk.irk, 
                        (p_event_data->keys)->irk,
                        MAX_WORDS_IRK);

                /* If bonded device address is resolvable random
                 * then store IRK to NVM 
                 */
                Nvm_Write(g_ht_data.central_device_irk.irk, 
                          MAX_WORDS_IRK, 
                          NVM_OFFSET_SM_IRK);
            }

        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      handleSignalSmPairingAuthInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_PAIRING_AUTH_IND. This message will
 *      only be received when the peer device is initiating 'Just Works' 
 *      pairing.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
        {
            /* Authorise the pairing request if the application is NOT bonded */
            if(!g_ht_data.bonded)
            {
                SMPairingAuthRsp(p_event_data->data, TRUE);
            }
            else /* Otherwise Reject the pairing request */
            {
                SMPairingAuthRsp(p_event_data->data, FALSE);
            }
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);
        break;
    }
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmSimplePairingCompleteInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_SIMPLE_PAIRING_COMPLETE_IND
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalSmSimplePairingCompleteInd(
                                 SM_SIMPLE_PAIRING_COMPLETE_IND_T *p_event_data)
{

    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
        {
            if(p_event_data->status == sys_status_success)
            {
                /* Store bonded host information to NVM. This includes
                 * application and services specific information
                 */
                g_ht_data.bonded = TRUE;
                g_ht_data.bonded_bd_addr = p_event_data->bd_addr;

                /* Store bonded host typed bd address to NVM */

                /* Write one word bonded flag */
                Nvm_Write((uint16*)&g_ht_data.bonded, 
                          sizeof(g_ht_data.bonded), 
                          NVM_OFFSET_BONDED_FLAG);

                /* Write typed bd address of bonded host */
                Nvm_Write((uint16*)&g_ht_data.bonded_bd_addr, 
                          sizeof(TYPED_BD_ADDR_T), 
                          NVM_OFFSET_BONDED_ADDR);

                /* Configure white list with the Bonded host address only 
                 * if the connected host doesn't support random resolvable
                 * address
                 */
                if(!GattIsAddressResolvableRandom(&g_ht_data.bonded_bd_addr))
                {
                    /* It is important to note that this application 
                     * doesn't support reconnection address. In future, if 
                     * the application is enhanced to support Reconnection 
                     * Address, make sure that we don't add reconnection 
                     * address to white list
                     */
                    if(LsAddWhiteListDevice(&g_ht_data.bonded_bd_addr) !=
                        ls_err_none)
                    {
                        ReportPanic(app_panic_add_whitelist);
                    }

                }

                /* If the devices are bonded then send notification to all 
                 * registered services for the same so that they can store
                 * required data to NVM.
                 */

                HealthThermoBondingNotify();

                BatteryBondingNotify();
            }
            else
            {
                /* Pairing has failed.
                 * 1. If pairing has failed due to repeated attempts, the 
                 *    application should immediately disconnect the link.
                 * 2. The application was bonded and pairing has failed.
                 *    Since the application was using whitelist, so the remote 
                 *    device has same address as our bonded device address.
                 *    The remote connected device may be a genuine one but 
                 *    instead of using old keys, wanted to use new keys. We 
                 *    don't allow bonding again if we are already bonded but we
                 *    will give some time to the connected device to encrypt the
                 *    link using the old keys. if the remote device encrypts the
                 *    link in that time, it's good. Otherwise we will disconnect
                 *    the link.
                 */
                 if(p_event_data->status == sm_status_repeated_attempts)
                 {
                    AppSetState(app_state_disconnecting);
                 }
                 else if(g_ht_data.bonded)
                 {
                    g_ht_data.encrypt_enabled = FALSE;
                    g_ht_data.bonding_reattempt_tid = 
                                         TimerCreate(
                                               BONDING_CHANCE_TIMER,
                                               TRUE, 
                                               handleBondingChanceTimerExpiry);
                 }
            }
        }
        break;

        default:
            /* Firmware may send this signal after disconnection. So don't 
             * panic but ignore this signal.
             */
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLMEncryptionChange
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_ENCRYPTION_CHANGE
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLMEncryptionChange(
                    HCI_EV_DATA_ENCRYPTION_CHANGE_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
        {

            if(p_event_data->status == sys_status_success)
            {
                g_ht_data.encrypt_enabled = p_event_data->enc_enable;

                if(g_ht_data.encrypt_enabled)
                {
                    
                    /* Delete the bonding chance timer */
                    TimerDelete(g_ht_data.bonding_reattempt_tid);
                    g_ht_data.bonding_reattempt_tid = TIMER_INVALID;
                
                    /* Update battery status at every connection instance. It 
                     * may not be worth updating timer more often, but again 
                     * it will primarily depend upon application requirements 
                     */
                    BatteryUpdateLevel(g_ht_data.st_ucid);

                    /* Start Temperature measurement timer to periodically
                     * send measured readings to the connected host 
                     */
                    htTempMeasTimerHandler(TIMER_INVALID);

                }

            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmDivApproveInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_DIV_APPROVE_IND.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_ht_data.state)
    {
        
        /* Request for approval from application comes only when pairing is not
         * in progress
         */
        case app_state_connected:
        {
            sm_div_verdict approve_div = SM_DIV_REVOKED;
            
            /* Check whether the application is still bonded (bonded flag gets
             * reset upon 'connect' button press by the user). Then check 
             * whether the diversifier is the same as the one stored by the 
             * application
             */
            if(g_ht_data.bonded)
            {
                if(g_ht_data.diversifier == p_event_data->div)
                {
                    approve_div = SM_DIV_APPROVED;
                }
            }

            SMDivApproval(p_event_data->cid, approve_div);
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;

    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateCfm
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_CFM.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLsConnParamUpdateCfm(
                            LS_CONNECTION_PARAM_UPDATE_CFM_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
        {
            /* Received in response to the L2CAP_CONNECTION_PARAMETER_UPDATE 
             * request sent from the slave after encryption is enabled. If 
             * the request has failed, the device should again send the same 
             * request only after Tgap(conn_param_timeout). Refer 
             * Bluetooth 4.0 spec Vol 3 Part C, Section 9.3.9 and profile spec.
             */
            if ((p_event_data->status != ls_err_none) &&
                    (g_ht_data.num_conn_update_req < 
                    MAX_NUM_CONN_PARAM_UPDATE_REQS))
            {
                /* Delete timer if running */
                TimerDelete(g_ht_data.con_param_update_tid);

                g_ht_data.con_param_update_tid = TimerCreate(
                                             GAP_CONN_PARAM_TIMEOUT,
                                             TRUE, requestConnParamUpdate);
                g_ht_data.cpu_timer_value = GAP_CONN_PARAM_TIMEOUT;
            }
        }
        break;

        default:
        /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmConnectionUpdate
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_UPDATE.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
static void handleSignalLmConnectionUpdate(
                                       LM_EV_CONNECTION_UPDATE_T* p_event_data)
{
    switch(g_ht_data.state)
    {
        case app_state_connected:
        case app_state_disconnecting:
        {
            /* Store the new connection parameters. */
            g_ht_data.conn_interval = p_event_data->data.conn_interval;
            g_ht_data.conn_latency = p_event_data->data.conn_latency;
            g_ht_data.conn_timeout = p_event_data->data.supervision_timeout;
        }
        break;

        default:
            /* Connection parameter update indication received in unexpected
             * application state.
             */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_IND
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLsConnParamUpdateInd(
                                 LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data)
{

    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {

        case app_state_connected:
        {
            /* Delete timer if running */
            TimerDelete(g_ht_data.con_param_update_tid);
            g_ht_data.con_param_update_tid = TIMER_INVALID;
            g_ht_data.cpu_timer_value = 0;
            
            /* The application had already received the new connection 
             * parameters while handling event LM_EV_CONNECTION_UPDATE.Check if
             * new parameters comply with application preferred parameters. 
             * If not, application shall trigger Connection parameter update 
             * procedure.
             */
            if(g_ht_data.conn_interval < PREFERRED_MIN_CON_INTERVAL ||
               g_ht_data.conn_interval > PREFERRED_MAX_CON_INTERVAL
#if PREFERRED_SLAVE_LATENCY
               || g_ht_data.conn_latency < PREFERRED_SLAVE_LATENCY
#endif
              )
            {
                /* Set the num of conn update attempts to zero */
                g_ht_data.num_conn_update_req = 0;

                /* Start timer to trigger Connection Parameter Update 
                 * procedure 
                 */
                g_ht_data.con_param_update_tid = TimerCreate(
                                         GAP_CONN_PARAM_TIMEOUT,
                                         TRUE, requestConnParamUpdate);
                g_ht_data.cpu_timer_value = GAP_CONN_PARAM_TIMEOUT;

            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAccessInd
 *
 *  DESCRIPTION
 *      This function handles GATT_ACCESS_IND message for attributes 
 *      maintained by the application.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattAccessInd(GATT_ACCESS_IND_T *p_event_data)
{

   
    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
        {

            /* GATT_ACCESS_IND indicates that the central device is still disco-
             * -vering services. So, restart the connection parameter update 
             * timer
             */
             if(g_ht_data.cpu_timer_value == TGAP_CPC_PERIOD && 
                g_ht_data.con_param_update_tid != TIMER_INVALID)
             {
                TimerDelete(g_ht_data.con_param_update_tid);
                g_ht_data.con_param_update_tid = TimerCreate(TGAP_CPC_PERIOD,
                                                 TRUE, requestConnParamUpdate);
             }
             
            /* Received GATT ACCESS IND with write access */
            if(p_event_data->flags == 
                (ATT_ACCESS_WRITE | 
                 ATT_ACCESS_PERMISSION | 
                 ATT_ACCESS_WRITE_COMPLETE))
            {
               
                HandleAccessWrite(p_event_data);

               

                /* Check if indications are configured on Temperature 
                 * Measurement characteristic of Health Thermometer service
                 */
                if(g_ht_data.encrypt_enabled)
                {
                    /* Delete thermometer measurement timer */
                    TimerDelete(g_ht_data.app_tid);
                    g_ht_data.app_tid = TIMER_INVALID;

                    /* Send the temperature reading and start temperature 
                     * measurement timer to periodically send measured readings 
                     * to the connected host 
                     */
                    htTempMeasTimerHandler(TIMER_INVALID);
                }
            }
            /* Received GATT ACCESS IND with read access */
            else if(p_event_data->flags == 
                (ATT_ACCESS_READ | 
                ATT_ACCESS_PERMISSION))
            {
                HandleAccessRead(p_event_data);
            }
            else
            {
                GattAccessRsp(p_event_data->cid, p_event_data->handle, 
                              gatt_status_request_not_supported,
                              0, NULL);
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleBondingChanceTimerExpiry
 *
 *  DESCRIPTION
 *      This function is handle the expiry of bonding chance timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 
*----------------------------------------------------------------------------*/
static void handleBondingChanceTimerExpiry(timer_id tid)
{
    if(g_ht_data.bonding_reattempt_tid== tid)
    {
        g_ht_data.bonding_reattempt_tid= TIMER_INVALID;
        /* The bonding chance timer has expired. This means the remote has not
         * encrypted the link using old keys. Disconnect the link.
         */
        AppSetState(app_state_disconnecting);
    }/* Else it may be due to some race condition. Ignore it. */
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmDisconnectComplete
 *
 *  DESCRIPTION
 *      This function handles LM Disconnect Complete event which is received
 *      at the completion of disconnect procedure triggered either by the 
 *      device or remote host or because of link loss 
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLmDisconnectComplete(
                HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data)
{

    /* Delete the bonding chance timer */
    TimerDelete(g_ht_data.bonding_reattempt_tid);
    g_ht_data.bonding_reattempt_tid = TIMER_INVALID;

    /* Reset the connection parameter variables. */
    g_ht_data.conn_interval = 0;
    g_ht_data.conn_latency = 0;
    g_ht_data.conn_timeout = 0;
    
    /* LM_EV_DISCONNECT_COMPLETE event can have following disconnect 
     * reasons:
     *
     * HCI_ERROR_CONN_TIMEOUT - Link Loss case
     * HCI_ERROR_CONN_TERM_LOCAL_HOST - Disconnect triggered by device
     * HCI_ERROR_OETC_* - Other end (i.e., remote host) terminated connection
     */

    /*Handling signal as per current state */
    switch(g_ht_data.state)
    {
        case app_state_connected:
            /* Initialise health thermometer data instance */
            htDataInit();

            /* FALLTHROUGH */

        case app_state_disconnecting:
        {

            /* Link Loss Case */
            if(p_event_data->reason == HCI_ERROR_CONN_TIMEOUT)
            {
                /* Start undirected advertisements by moving to 
                 * app_state_fast_advertising state
                 */
                AppSetState(app_state_fast_advertising);
            }
            else if(p_event_data->reason == HCI_ERROR_CONN_TERM_LOCAL_HOST)
            {

                if(g_ht_data.state == app_state_connected)
                {
                    /* It is possible to receive LM_EV_DISCONNECT_COMPLETE 
                     * event in app_state_connected state at the expiry of 
                     * lower layers ATT/SMP timer leading to disconnect
                     */

                    /* Start undirected advertisements by moving to 
                     * app_state_fast_advertising state
                     */
                    AppSetState(app_state_fast_advertising);
                }
                else
                {
                    /* Case when application has triggered disconnect */

                    if(g_ht_data.bonded)
                    {
                        /* If the device is bonded and host uses resolvable 
                         * random address, the device initiates disconnect 
                         * procedure if it gets reconnected to a different host,
                         * in which case device should trigger fast 
                         * advertisements after disconnecting from the last 
                         * connected host.
                         */
                        if(GattIsAddressResolvableRandom(
                                        &g_ht_data.bonded_bd_addr) &&
                           (SMPrivacyMatchAddress(&g_ht_data.con_bd_addr,
                                        g_ht_data.central_device_irk.irk,
                                        MAX_NUMBER_IRK_STORED, 
                                        MAX_WORDS_IRK) < 0))
                        {
                            AppSetState(app_state_fast_advertising);
                        }
                        else
                        {
                            /* Else move to app_state_idle state because of 
                             * user action
                             */
                            AppSetState(app_state_idle);
                        }
                    }
                    else /* Case of Bonding/Pairing removal */
                    {
                        /* Start undirected advertisements by moving to 
                         * app_state_fast_advertising state
                         */
                        AppSetState(app_state_fast_advertising);
                    }
                }

            }
            else /* Remote user terminated connection case */
            {
                /* If the device has not bonded but disconnected, it may just 
                 * have discovered the services supported by the application or 
                 * read some un-protected characteristic value like device name 
                 * and disconnected. The application should be connectable 
                 * because the same remote device may want to reconnect and 
                 * bond. If not the application should be discoverable by other 
                 * devices.
                 */
                if(!g_ht_data.bonded)
                {
                    AppSetState(app_state_fast_advertising);
                }
                else /* Case when disconnect is triggered by a bonded Host */
                {
                    AppSetState(app_state_idle);
                }
            }

        }
        break;
        
        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

#ifdef NVM_TYPE_FLASH
/*----------------------------------------------------------------------------*
 *  NAME
 *      WriteApplicationAndServiceDataToNVM
 *
 *  DESCRIPTION
 *      This function writes the application data to NVM. This function should 
 *      be called on getting nvm_status_needs_erase
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
extern void WriteApplicationAndServiceDataToNVM(void)
{
    uint16 nvm_sanity = 0xffff;
    nvm_sanity = NVM_SANITY_MAGIC;

    /* Write NVM sanity word to the NVM */
    Nvm_Write(&nvm_sanity, sizeof(nvm_sanity), NVM_OFFSET_SANITY_WORD);

    /* Write Bonded flag to NVM. */
    Nvm_Write((uint16*)&g_ht_data.bonded, 
               sizeof(g_ht_data.bonded),
               NVM_OFFSET_BONDED_FLAG);


    /* Write Bonded address to NVM. */
    Nvm_Write((uint16*)&g_ht_data.bonded_bd_addr,
              sizeof(TYPED_BD_ADDR_T),
              NVM_OFFSET_BONDED_ADDR);

    /* Write the diversifier to NVM */
    Nvm_Write(&g_ht_data.diversifier,
                sizeof(g_ht_data.diversifier),
                NVM_OFFSET_SM_DIV);

    /* Store the IRK to NVM */
    Nvm_Write(g_ht_data.central_device_irk.irk,
                MAX_WORDS_IRK,
                NVM_OFFSET_SM_IRK);

    /* Write GAP service data into NVM */
    WriteGapServiceDataInNVM();

    /* Write Health Thermometer service data into NVM */
    WriteHealthThermometerServiceDataInNvm();

    /* Write Battery service data into NVM */
    WriteBatteryServiceDataInNvm();
}
#endif /* NVM_TYPE_FLASH */


/*----------------------------------------------------------------------------*
 *  NAME
 *      ReportPanic
 *
 *  DESCRIPTION
 *      This function calls firmware panic routine and gives a single point 
 *      of debugging any application level panics
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void ReportPanic(app_panic_code panic_code)
{
    /* Raise panic */
    Panic(panic_code);
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleShortButtonPress
 *
 *  DESCRIPTION
 *      This function contains handling of short button press. If connected,
 *      device starts measurement timer to send periodic measurements to the 
 *      connected host else triggers advertisements
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandleShortButtonPress(uint8 *val)
{

    uint8 *data=val;
    if(!HealthThermoSendTempReading(g_ht_data.st_ucid, 
                                                    data))
    {
    }
   
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleExtraLongButtonPress
 *
 *  DESCRIPTION
 *      This function contains handling of extra long button press, which
 *      triggers pairing / bonding removal
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandleExtraLongButtonPress(timer_id tid)
{
    if(tid == g_app_hw_data.button_press_tid)
    {
        /* Re-initialise timer id */
        g_app_hw_data.button_press_tid = TIMER_INVALID;

        /* Sound three beeps to indicate pairing removal to user */
        SoundBuzzer(buzzer_beep_thrice);

        /* Remove bonding information*/

        /* The device will no more be bonded */
        g_ht_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&g_ht_data.bonded, 
                  sizeof(g_ht_data.bonded), 
                  NVM_OFFSET_BONDED_FLAG);


        switch(g_ht_data.state)
        {

            case app_state_connected:
            {
                /* Delete thermometer measurement timer */
                TimerDelete(g_ht_data.app_tid);
                g_ht_data.app_tid = TIMER_INVALID;

                /* Disconnect with the connected host before triggering 
                 * advertisements again for any host to connect. Application
                 * and services data related to bonding status will get 
                 * updated while exiting disconnecting state
                 */
                AppSetState(app_state_disconnecting);

                /* Reset and clear the whitelist */
                LsResetWhiteList();
            }
            break;

            case app_state_fast_advertising:
            case app_state_slow_advertising:
            {
                /* Initialise application and services data related to 
                 * for bonding status
                 */
                htDataInit();

                /* Set flag for pairing / bonding removal */
                g_ht_data.pairing_button_pressed = TRUE;

                /* Stop advertisements first as it may be making use of white 
                 * list. Once advertisements are stopped, reset the whitelist
                 * and trigger advertisements again for any host to connect
                 */
                GattStopAdverts();
            }
            break;

            case app_state_disconnecting:
            {
                /* Disconnect procedure on-going, so just reset the whitelist 
                 * and wait for procedure to get completed before triggering 
                 * advertisements again for any host to connect. Application
                 * and services data related to bonding status will get 
                 * updated while exiting disconnecting state
                 */
                LsResetWhiteList();
            }
            break;

            default: /* app_state_init / app_state_idle handling */
            {
                /* Initialise application and services data related to 
                 * for bonding status
                 */
                htDataInit();

                /* Reset and clear the whitelist */
                LsResetWhiteList();

                /* Start fast undirected advertisements */
                AppSetState(app_state_fast_advertising);
            }
            break;

        }

    } /* Else ignore timer */

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppSetState
 *
 *  DESCRIPTION
 *      This function is used to set the state of the application.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void AppSetState(app_state new_state)
{
    /* Check if the new state to be set is not the same as the present state
     * of the application. */
    app_state old_state = g_ht_data.state;
    
    if (old_state != new_state)
    {
        /* Handle exiting old state */
        switch (old_state)
        {
            case app_state_init:
                appInitExit();
            break;

            case app_state_disconnecting:
                /* Common things to do whenever application exits
                 * app_state_disconnecting state.
                 */

                /* Initialise application and used services data structure 
                 * while exiting Disconnecting state
                 */
                htDataInit();
            break;

            case app_state_fast_advertising:
            case app_state_slow_advertising:
                /* Common things to do whenever application exits
                 * APP_*_ADVERTISING state.
                 */
                appAdvertisingExit();
            break;

            case app_state_connected:
                /* Nothing to do */
            break;

            case app_state_idle:
                /* Nothing to do */
            break;

            default:
                /* Nothing to do */
            break;
        }

        /* Set new state */
        g_ht_data.state = new_state;

        /* Handle entering new state */
        switch (new_state)
        {
            case app_state_fast_advertising:
            {
                GattStartAdverts(TRUE);

                /* Indicate advertising mode by sounding two short beeps */
                SoundBuzzer(buzzer_beep_twice);
            }
            break;

            case app_state_slow_advertising:
                GattStartAdverts(FALSE);
            break;

            case app_state_idle:
                /* Sound long beep to indicate non connectable mode*/
                SoundBuzzer(buzzer_beep_long);
            break;

            case app_state_connected:
            {
                /* Common things to do whenever application enters
                 * app_state_connected state.
                 */

                /* Trigger SM Slave Security request only if the remote 
                 * host is not using resolvable random address
                 */
                if(!GattIsAddressResolvableRandom(&g_ht_data.con_bd_addr))
                {
                    SMRequestSecurityLevel(&g_ht_data.con_bd_addr);
                }

            }
            break;

            case app_state_disconnecting:
                GattDisconnectReq(g_ht_data.st_ucid);
            break;

            default:
            break;
        }
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppIsDeviceBonded
 *
 *  DESCRIPTION
 *      This function returns the status whether the connected device is 
 *      bonded or not.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern bool AppIsDeviceBonded(void)
{
    return g_ht_data.bonded;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppPowerOnReset
 *
 *  DESCRIPTION
 *      This function is called just after a power-on reset (including after
 *      a firmware panic).
 *
 *      NOTE: this function should only contain code to be executed after a
 *      power-on reset or panic. Code that should also be executed after an
 *      HCI_RESET should instead be placed in the reset() function.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void AppPowerOnReset(void)
{
    /* Configure the application constants */
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppInit
 *
 *  DESCRIPTION
 *      This function is called after a power-on reset (including after a
 *      firmware panic) or after an HCI Reset has been requested.
 *
 *      NOTE: In the case of a power-on reset, this function is called
 *      after app_power_on_reset.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void AppInit(sleep_state last_sleep_state)
{

    uint16 gatt_db_length = 0;
    uint16 *p_gatt_db = NULL;

    /* Initialise the application timers */
    TimerInit(MAX_APP_TIMERS, (void*)app_timers);
 
    /* Initialise GATT entity */
    GattInit();

    /* Install GATT Server support for the optional Write procedure
     * This is mandatory only if control point characteristic is supported. 
     */
    GattInstallServerWrite();

    /* Don't wakeup on UART RX line */
    SleepWakeOnUartRX(FALSE);

#ifdef NVM_TYPE_EEPROM
    /* Configure the NVM manager to use I2C EEPROM for NVM store */
    NvmConfigureI2cEeprom();
#elif NVM_TYPE_FLASH
    /* Configure the NVM Manager to use SPI flash for NVM store. */
    NvmConfigureSpiFlash();
#endif /* NVM_TYPE_EEPROM */

    Nvm_Disable();

    /* Battery Initialisation on Chip reset */
    BatteryInitChipReset();

    /* Initialize the gap data. Needs to be done before readPersistentStore */
    GapDataInit();

    /* Read persistent storage */
    readPersistentStore();

    /* Tell Security Manager module about the value it needs to initialize it's
     * diversifier to.
     */
    SMInit(g_ht_data.diversifier);

    /* Initialise application data structure */
    htDataInit();

    /* Initialise Health Thermometer H/W */
    HtInitHardware();

    /* Tell GATT about our database. We will get a GATT_ADD_DB_CFM event when
     * this has completed.
     */
    p_gatt_db = GattGetDatabase(&gatt_db_length);

    /* Initialise Health Thermometer State */
    g_ht_data.state = app_state_init;

    GattAddDatabaseReq(gatt_db_length, p_gatt_db);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessSystemEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a system event, such
 *      as a battery low notification, is received by the system.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void AppProcessSystemEvent(sys_event_id id, void *data)
{
    switch(id)
    {
        case sys_event_battery_low:
        {
            /* Battery low event received - notify the connected host. If 
             * not connected, the battery level will get notified when 
             * device gets connected again
             */
            if(g_ht_data.state == app_state_connected)
            {
                BatteryUpdateLevel(g_ht_data.st_ucid);
            }
        }
        break;

        case sys_event_pio_changed:
            HandlePIOChangedEvent((pio_changed_data*)data);
        break;

        default:
            /* Ignore anything else */
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessLmEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a LM-specific event is
 *      received by the system.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern bool AppProcessLmEvent(lm_event_code event_code, 
                              LM_EVENT_T *p_event_data)
{

    switch (event_code)
    {

        /* Handle events received from Firmware */

        case GATT_ADD_DB_CFM:
            /* Attribute database registration confirmation */
            handleSignalGattAddDbCfm((GATT_ADD_DB_CFM_T*)p_event_data);
        break;

        case GATT_CANCEL_CONNECT_CFM:
            /* Confirmation for the completion of GattCancelConnectReq()
             * procedure 
             */
            handleSignalGattCancelConnectCfm();
        break;

        case LM_EV_CONNECTION_COMPLETE:
            /* Handle the LM connection complete event. */
            handleSignalLmEvConnectionComplete((LM_EV_CONNECTION_COMPLETE_T*)
                                                                p_event_data);
        break;

        case GATT_CONNECT_CFM:
            /* Confirmation for the completion of GattConnectReq() 
             * procedure
             */
            handleSignalGattConnectCfm((GATT_CONNECT_CFM_T*)p_event_data);
        break;

        case SM_KEYS_IND:
            /* Indication for the keys and associated security information
             * on a connection that has completed Short Term Key Generation 
             * or Transport Specific Key Distribution
             */
            handleSignalSmKeysInd((SM_KEYS_IND_T *)p_event_data);
        break;

        case SM_PAIRING_AUTH_IND:
            /* Authorize or Reject the pairing request */
            handleSignalSmPairingAuthInd((SM_PAIRING_AUTH_IND_T*)p_event_data);
        break;

        case SM_SIMPLE_PAIRING_COMPLETE_IND:
            /* Indication for completion of Pairing procedure */
            handleSignalSmSimplePairingCompleteInd(
                (SM_SIMPLE_PAIRING_COMPLETE_IND_T *)p_event_data);
        break;

        case LM_EV_ENCRYPTION_CHANGE:
            /* Indication for encryption change event */
            handleSignalLMEncryptionChange(
            (HCI_EV_DATA_ENCRYPTION_CHANGE_T *)&p_event_data->enc_change.data);
        break;

        case SM_DIV_APPROVE_IND:
            /* Indication for SM Diversifier approval requested by F/W when 
             * the last bonded host exchange keys. Application may or may not
             * approve the diversifier depending upon whether the application 
             * is still bonded to the same host
             */
            handleSignalSmDivApproveInd((SM_DIV_APPROVE_IND_T *)p_event_data);
        break;

        /* Received in response to the LsConnectionParamUpdateReq() 
         * request sent from the slave after encryption is enabled. If 
         * the request has failed, the device should again send the same 
         * request only after Tgap(conn_param_timeout). Refer Bluetooth 4.0 
         * spec Vol 3 Part C, Section 9.3.9 and HID over GATT profile spec 
         * section 5.1.2.
         */
        case LS_CONNECTION_PARAM_UPDATE_CFM:
            handleSignalLsConnParamUpdateCfm((LS_CONNECTION_PARAM_UPDATE_CFM_T*)
                p_event_data);
        break;


        case LM_EV_CONNECTION_UPDATE:
            /* This event is sent by the controller on connection parameter 
             * update. 
             */
            handleSignalLmConnectionUpdate(
                            (LM_EV_CONNECTION_UPDATE_T*)p_event_data);
        break;


        case LS_CONNECTION_PARAM_UPDATE_IND:
            /* Indicates completion of remotely triggered Connection 
             * parameter update procedure
             */
            handleSignalLsConnParamUpdateInd(
                            (LS_CONNECTION_PARAM_UPDATE_IND_T *)p_event_data);
        break;

        case GATT_ACCESS_IND:
            /* Indicates that an attribute controlled directly by the
             * application (ATT_ATTR_IRQ attribute flag is set) is being 
             * read from or written to.
             */
            handleSignalGattAccessInd((GATT_ACCESS_IND_T *)p_event_data);
        break;

        case GATT_DISCONNECT_IND:
            /* Disconnect procedure triggered by remote host or due to 
             * link loss is considered complete on reception of 
             * LM_EV_DISCONNECT_COMPLETE event. So, it gets handled on 
             * reception of LM_EV_DISCONNECT_COMPLETE event.
             */
        break;

        case GATT_DISCONNECT_CFM:
            /* Confirmation for the completion of GattDisconnectReq()
             * procedure is ignored as the procedure is considered complete 
             * on reception of LM_EV_DISCONNECT_COMPLETE event. So, it gets 
             * handled on reception of LM_EV_DISCONNECT_COMPLETE event.
             */
        break;

        case LM_EV_DISCONNECT_COMPLETE:
        {
            /* Disconnect procedures either triggered by application or remote
             * host or link loss case are considered completed on reception 
             * of LM_EV_DISCONNECT_COMPLETE event
             */
             handleSignalLmDisconnectComplete(
                    &((LM_EV_DISCONNECT_COMPLETE_T *)p_event_data)->data);
        }
        break;

        default:
            /* Ignore any other event */ 
        break;

    }

    return TRUE;
}
