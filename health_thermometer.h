/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      health_thermometer.h
 *
 *  DESCRIPTION
 *      This file defines a simple implementation of Health Thermometer service
 *
 *****************************************************************************/

#ifndef __HEALTH_THERMOMETER_H__
#define __HEALTH_THERMOMETER_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bluetooth.h>
#include <timer.h>

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Maximum number of words in central device IRK */
#define MAX_WORDS_IRK                       (8)

/*============================================================================*
 *  Public Data Types
 *============================================================================*/

typedef enum
{

    /* Application Initial State */
    app_state_init = 0,

    /* Enters when fast undirected advertisements are configured */
    app_state_fast_advertising,

    /* Enters when slow Undirected advertisements are configured */
    app_state_slow_advertising,

    /* Enters when connection is established with the host */
    app_state_connected,

    /* Enters when disconnect is initiated by the application */
    app_state_disconnecting,

    /* Enters when the application is not connected to remote host */
    app_state_idle

} app_state;


/* Structure defined for Central device IRK */
typedef struct
{
    uint16                         irk[MAX_WORDS_IRK];

} CENTRAL_DEVICE_IRK_T;

/* Health Thermometer application data structure */
typedef struct
{
    /* Application state */
    app_state                      state;

    /* Store timer id while doing 'UNDIRECTED ADVERTS' and periodic 
     * temperature measurements in CONNECTED' states.
     */
    timer_id                       app_tid;

    /* TYPED_BD_ADDR_T of the host to which device is connected */
    TYPED_BD_ADDR_T                con_bd_addr;

    /* Track the UCID as Clients connect and disconnect */
    uint16                         st_ucid;

    /* Boolean flag to indicated whether the device is bonded */
    bool                           bonded;

    /* TYPED_BD_ADDR_T of the host to which device is bonded.*/
    TYPED_BD_ADDR_T                bonded_bd_addr;

    /* Diversifier associated with the LTK of the bonded device */
    uint16                         diversifier;

    /* Store timer id for Connection Parameter Update timer in Connected 
     * state
     */
    timer_id                       con_param_update_tid;

    /* Connection Parameter Update timer value. Upon a connection, it's started
     * for a period of TGAP_CPP_PERIOD, upon the expiry of which it's restarted
     * for TGAP_CPC_PERIOD. When this timer is running, if a GATT_ACCESS_IND is
     * received, it means, the central device is still doing the service discov-
     * -ery procedure. So, the connection parameter update timer is deleted and
     * recreated. Upon the expiry of this timer, a connection parameter update
     * request is sent to the central device.
     */
    uint32                         cpu_timer_value;

    /* Central Private Address Resolution IRK  Will only be used when
     * central device used resolvable random address. 
     */
    CENTRAL_DEVICE_IRK_T           central_device_irk;

    /* Variable to keep track of number of connection parameter update 
     * requests made 
     */
    uint8                          num_conn_update_req;

    /* Boolean flag indicating whether encryption is enabled with the 
     * bonded host
     */
    bool                           encrypt_enabled;

    /* Boolean flag set to indicate pairing button press */
    bool                           pairing_button_pressed;

    /* This timer will be used if the application is already bonded to the 
     * remote host address but the remote device wanted to rebond which we had 
     * declined. In this scenario, we give ample time to the remote device to 
     * encrypt the link using old keys. If remote device doesn't encrypt the 
     * link, we will disconnect the link on this timer expiry.
     */
    timer_id                       bonding_reattempt_tid;

    /* Varibale to store the current connection interval being used. */
    uint16                         conn_interval;

    /* Variable to store the current slave latency. */
    uint16                         conn_latency;

    /*Variable to store the current connection timeout value. */
    uint16                         conn_timeout;


} HT_DATA_T;

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/

/* Health Thermometer application data instance */
extern HT_DATA_T g_ht_data;


/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function contains handling of short button press */
extern void HandleShortButtonPress(uint8 *val);

/* This function contains handling of extra long button press */
extern void HandleExtraLongButtonPress(timer_id tid);

/* This function is used to set the state of the application */
extern void AppSetState(app_state new_state);


#endif /* __HEALTH_THERMOMETER_H__ */
