/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      app_gatt.h
 *
 *  DESCRIPTION
 *      Header definitions for common application attributes
 *
 ******************************************************************************/

#ifndef __APP_GATT_H__
#define __APP_GATT_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Invalid UCID indicating we are not currently connected */
#define GATT_INVALID_UCID                    (0xFFFF)

#if defined(CSR100x)
/* AD Type for Appearance */
#define AD_TYPE_APPEARANCE                                (0x19)
#endif

/* Invalid Attribute Handle */
#define INVALID_ATT_HANDLE                   (0x0000)

/* Extract low order byte of 16-bit UUID */
#define LE8_L(x)                             ((x) & 0xff)

/* Extract low order byte of 16-bit UUID */
#define LE8_H(x)                             (((x) >> 8) & 0xff)

/* Maximum Length of Device Name 
 * Note: Do not increase device name length beyond (DEFAULT_ATT_MTU -3 = 20) 
 * octets as GAP service at the moment doesn't support handling of Prepare 
 * write and Execute write procedures.
 */
#define DEVICE_NAME_MAX_LENGTH               (20)


/* Timer value for remote device to re-encrypt the link using old keys */
#define BONDING_CHANCE_TIMER                 (30*SECOND)

/* GATT ERROR codes: 
 * Going forward the following codes will be included in the firmware APIs.
 */

/* This error codes should be returned when a remote connected device writes a 
 * configuration which the application does not support.
 */
#define gatt_status_desc_improper_config     (STATUS_GROUP_GATT+ 0xFD)

/* The following error codes shall be returned when a procedure is already 
 * ongoing and the remote connected device request for the same procedure 
again.
 */
#define gatt_status_proc_in_progress         (STATUS_GROUP_GATT+ 0xFE)

/* This error code shall be returned if the written value is out of the 
 * supported range.
 */
#define gatt_status_att_val_oor              (STATUS_GROUP_GATT+ 0xFF)


/*============================================================================*
 *  Public Data Types
 *============================================================================*/

/* GATT Client Characteristic Configuration Value [Ref GATT spec, 3.3.3.3]
 * Client Configuration is a bit field value where each bit corresponds to a 
 * particular configuration
 * Bit- Configuration Mapping
 * Bit 0 - Notifications
 * Bit 1 - Indications
 */
typedef enum
{
    gatt_client_config_none                  = 0x0000,
    gatt_client_config_notification          = 0x0001,
    gatt_client_config_indication            = 0x0002,
    gatt_client_config_reserved              = 0xFFF4

} gatt_client_config;

/* Application defined panic codes 
 * Persistent storage which is used to hold panic code is intialized to zero, 
 * so the application shall not use 0 for panic codes
 */
typedef enum
{
    /* Failure while setting advertisement parameters */
    app_panic_set_advert_params = 1,

    /* Failure while setting advertisement data */
    app_panic_set_advert_data,
    
    /* Failure while setting scan response data */
    app_panic_set_scan_rsp_data,

    /* Failure while establishing connection */
    app_panic_connection_est,

    /* Failure while registering GATT DB with firmware */
    app_panic_db_registration,

    /* Failure while reading NVM */
    app_panic_nvm_read,

    /* Failure while writing NVM */
    app_panic_nvm_write,

    /* Failure while reading Tx Power Level */
    app_panic_read_tx_pwr_level,

    /* Failure while deleting device from whitelist */
    app_panic_delete_whitelist,

    /* Failure while adding device to whitelist */
    app_panic_add_whitelist,

    /* Failure while triggering connection parameter update procedure */
    app_panic_con_param_update,

    /* Event received in an unexpected application state */
    app_panic_invalid_state,

    /* Unexpected beep type */
    app_panic_unexpected_beep_type,

    /* Failure while setting advertisement parameters */
    app_panic_gap_set_mode,


    /* Not supported UUID*/
    app_panic_uuid_not_supported,
    
    /* Failure while setting scan parameters */
    app_panic_set_scan_params,

    /* Failure while erasing NVM */
    app_panic_nvm_erase,

}app_panic_code;



/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function returns the status whether the connected device is 
 * bonded or not
 */
extern bool AppIsDeviceBonded(void);

/* This function calls firmware panic routine and gives a single point 
 * of debugging any application level panics
 */
extern void ReportPanic(app_panic_code panic_code);

#ifdef NVM_TYPE_FLASH
/* This function writes the application data to NVM. This function should 
 * be called on getting nvm_status_needs_erase
 */
extern void WriteApplicationAndServiceDataToNVM(void);
#endif /* NVM_TYPE_FLASH */

#endif /* __APP_GATT_H__ */
