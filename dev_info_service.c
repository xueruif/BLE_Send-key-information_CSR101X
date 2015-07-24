/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      dev_info_service.c
 *
 *  DESCRIPTION
 *      This file defines routines for using device info service.
 *
 *  NOTES
 *      This file is to be included if dev_info_db.db files contains
 *      System Id characteristic
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt_prim.h>
#include <bluetooth.h>
#include <config_store.h>
#include <gatt.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt_db.h"
#include "dev_info_service.h"

/*============================================================================*
 *  Private Data Types
 *============================================================================*/

/* System Id : System Id has two fields;
 * Manufacturer Identifier           : The Company Identifier is concatenated 
 *                                     with 0xFFFE
 * Organizationally Unique identifier: Company Assigned Identifier of the
 *                                     Bluetooth Address
 *
 *
 * See following web link for definition of system Id 
 * http://developer.bluetooth.org/gatt/characteristics/Pages/
 * CharacteristicViewer.aspx?u=org.bluetooth.characteristic.system_id.xml 
 */
typedef struct
{
    /* System Id size is 8 octets */
    uint8 word[8];
} SYSTEM_ID_T;

typedef struct
{
    /* System Id of device info service */
    SYSTEM_ID_T system_id;
} DEV_INFO_DATA_T;

/*============================================================================*
 *  Private Data
 *============================================================================*/

/* GAP service data structure */
DEV_INFO_DATA_T g_dev_info_data;

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Bytes have been reversed */
#define SYSTEM_ID_FIXED_CONSTANT    (0xFFFE)
#define SYSTEM_ID_LENGTH            (8)

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

/* This function calculates the system Id based on the bluetooth address of the
 * device.
 */
static bool getSystemId(SYSTEM_ID_T * sys_id);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      getSystemId
 *
 *  DESCRIPTION
 *      This function calculates the System Id based on the bluetooth 
 *      address of the device.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static bool getSystemId(SYSTEM_ID_T * sys_id)
{
    BD_ADDR_T bdaddr;

    if(CSReadBdaddr(&bdaddr))
    {
        /* Manufacturer identifier */
        sys_id->word[0] = (SYSTEM_ID_FIXED_CONSTANT >> 8);
        sys_id->word[1] = SYSTEM_ID_FIXED_CONSTANT;
        sys_id->word[2] = (bdaddr.lap >> 16);
        sys_id->word[3] = (bdaddr.lap >> 8);
        sys_id->word[4] = bdaddr.lap;

        /* Company Unique identifier */
        sys_id->word[5] = (bdaddr.nap >> 8);
        sys_id->word[6] = bdaddr.nap;
        sys_id->word[7] = bdaddr.uap;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/


/*----------------------------------------------------------------------------*
 *  NAME
 *      DeviceInfoHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles read operation on device info service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void DeviceInfoHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  *p_value = NULL;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {
        case HANDLE_DEVICE_INFO_SYSTEM_ID:
        {
            /*System Id read has been requested */
            length = SYSTEM_ID_LENGTH; 

            if(getSystemId(&g_dev_info_data.system_id))
            {
                p_value = (uint8 *)(&g_dev_info_data.system_id);
            }
            else
            {
                rc = gatt_status_unlikely_error;
            }
        }
        break;
        
        default:
        {
            /* Let firmware handle the request. */
            rc = gatt_status_irq_proceed;
        }
        break;
    }

    /* Send response indication */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, p_value);
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      DeviceInfoCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the device info 
 *      service
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/

extern bool DeviceInfoCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_DEVICE_INFO_SERVICE) &&
            (handle <= HANDLE_DEVICE_INFO_SERVICE_END))
            ? TRUE : FALSE;
}

