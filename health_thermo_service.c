/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      health_thermo_service.c
 *
 *  DESCRIPTION
 *      This file defines routines for using Health Thermometer service.
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt.h>
#include <gatt_prim.h>
#include <buf_utils.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt.h"
#include "health_thermo_service.h"
#include "nvm_access.h"
#include "app_gatt_db.h"

/*============================================================================*
 *  Private Data Types
 *============================================================================*/

/* Health Thermometer service data type */
typedef struct
{

    /* Flag for pending indication confirm */
    bool                    ind_cfm_pending;

    /* Client configuration for Temperature Measurement characteristic */
    gatt_client_config      temp_client_config;

    /* Offset at which Health Thermometer data is stored in NVM */
    uint16                  nvm_offset;

} HT_SERV_DATA_T;

typedef struct
{
   uint8 sendcnt;
   uint8 heartcnt;
   uint8 ghgcnt;
   uint8 anjstate;
   uint8 batlevel;
} HT_SR_DATA;
/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Health Thermometer service data instance */
static HT_SERV_DATA_T g_ht_serv_data;

uint8 send_count=0;

static HT_SR_DATA g_ht_sr_data;
/*============================================================================*
 *  Private Definitions
 *===========================================================================*/

/* Number of words of NVM memory used by Health Thermometer service */
#define HEALTH_THERMO_SERVICE_NVM_MEMORY_WORDS      (1)

/* The offset of data being stored in NVM for Health Thermometer service.
 * This offset is added to Health Thermometer service offset to NVM region 
 * to get the absolute offset at which this data is stored in NVM
 */
#define HEALTH_THERMO_NVM_TEMP_CLIENT_CONFIG_OFFSET (0)


/* Maximum Temperature Measurement size - This application is not 
 * supporting time stamp and uses separate characteristic for 
 * temperature type 
 */
#define MAX_TEMP_MEAS_SIZE                          (5)

/* Flags for Temp measurement information.
 * For details on these values, refer to http://developer.bluetooth.org/gatt/
 * characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.
 * characteristic.temperature_measurement.xml
 */
#define TEMP_MEAS_FLAGS_NONE                        (0x00)

#define TEMP_MEAS_UNIT_CELSIUS                      (0x00)
#define TEMP_MEAS_UNIT_FAHRENHEIT                   (0x01)
#define TEMP_MEAS_TIME_STAMP_PRESENT                (0x02)
#define TEMP_MEAS_TEMP_TYPE_PRESENT                 (0x04)



/*============================================================================*
 *  Public Function Implementations
 *===========================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise Health Thermometer service data 
 *      structure.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HealthThermoDataInit(void)
{
    if(!AppIsDeviceBonded())
    {
        /* Initialise Temperature Characteristic Client Configuration 
         * only if device is not bonded
         */
        g_ht_serv_data.temp_client_config = gatt_client_config_none;
    }
/*(modification  É¾µô)
    g_ht_serv_data.ind_cfm_pending =FALSE;
*/
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles read operation on health thermometer service 
 *      attributes maintained by the application and responds with the 
 *      GATT_ACCESS_RSP message.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HealthThermoHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  val[5]; 
    uint8 *p_value = NULL;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {
        case HANDLE_HT_TEMP_MEASUREMENT:
        {
            /* Reading TEMP level */
            length = 5; /* 5 Octet */
            val[0] = g_ht_sr_data.sendcnt;
            val[1] = g_ht_sr_data.heartcnt;            
            val[2] = g_ht_sr_data.ghgcnt;
            val[3] = g_ht_sr_data.anjstate;
            val[4] = g_ht_sr_data.batlevel;
        }
        break;
        case HANDLE_HT_TEMP_MEAS_C_CFG:
        {
            p_value = val;
            length = 2; /* Two Octets */

            BufWriteUint16((uint8 **)&p_value, 
                g_ht_serv_data.temp_client_config);
        }
        break;

        default:
        {
            /* Let firmware handle the request */
            rc = gatt_status_irq_proceed;
        }
        break;

    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, val);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles write operation on health thermometer service 
 *      attributes maintained by the application.and responds with the 
 *      GATT_ACCESS_RSP message.
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/

extern void HealthThermoHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    uint8 *p_value = p_ind->value;
    uint16 client_config;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {
        case HANDLE_HT_TEMP_MEAS_C_CFG:
        {
            client_config = BufReadUint16(&p_value);


            /* Client Configuration is bit field value so ideally bitwise 
             * comparison should be used but since the application supports only 
             * indications, direct comparison is being used.
             ????????????????????????????????????????????indication????????
            if((client_config == gatt_client_config_indication) ||
               (client_config == gatt_client_config_none))
               */
            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))            
            {
                g_ht_serv_data.temp_client_config = client_config;

                /* Write Temperature Client configuration to NVM if the 
                 * device is bonded.
                 */
                if(AppIsDeviceBonded())
                {
                     Nvm_Write(&client_config,
                              sizeof(client_config),
                              g_ht_serv_data.nvm_offset + 
                              HEALTH_THERMO_NVM_TEMP_CLIENT_CONFIG_OFFSET);
                }
            }
            else
            {
              

                /* Return Error as only Notifications are supported */
                rc = gatt_status_desc_improper_config;
            }

        }
        break;

        default:
            rc = gatt_status_write_not_permitted;
        break;
    }

    /* Send ACCESS RESPONSE */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoSendTempReading
 *
 *  DESCRIPTION
 *      This function is used to send temperature reading as an indication 
 *      to the connected host.
 *
 *  RETURNS
 *      Boolean: TRUE (If temperature reading is indicated to the connected 
 *               host) OR
 *               FALSE (If temperature reading could not be indicated to the 
 *               connected host)
 *
 *---------------------------------------------------------------------------*/

extern bool HealthThermoSendTempReading(uint16 ucid, uint8 *value)
{
    if(send_count==0xFF)
    {
        send_count=0;
    }
    else
    {
        send_count+=1;
    }
    value[0]=send_count;
    g_ht_sr_data.sendcnt = send_count;
    g_ht_sr_data.heartcnt = value[1];
    g_ht_sr_data.ghgcnt = value[2];
    g_ht_sr_data.anjstate = value[3];
    g_ht_sr_data.batlevel = value[4];
    if((ucid != GATT_INVALID_UCID) &&
       (g_ht_serv_data.temp_client_config & gatt_client_config_notification))
    {
        GattCharValueNotification(ucid,
                HANDLE_HT_TEMP_MEASUREMENT, 
                MAX_TEMP_MEAS_SIZE, value);

        return TRUE;

     }

    return FALSE;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoRegIndicationCfm
 *
 *  DESCRIPTION
 *      This function is used to set the status of pending confirmation for the 
 *      transmitted temperature measurement indications
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------

extern void HealthThermoRegIndicationCfm(bool ind_state)
{
    g_ht_serv_data.ind_cfm_pending = ind_state;
}
*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read health thermometer service specific data 
 *      stored in NVM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HealthThermoReadDataFromNVM(uint16 *p_offset)
{

    g_ht_serv_data.nvm_offset = *p_offset;

    /* Read NVM only if devices are bonded */
    if(AppIsDeviceBonded())
    {

        /* Read Termperature Client Configuration */
        Nvm_Read((uint16*)&g_ht_serv_data.temp_client_config,
                 sizeof(g_ht_serv_data.temp_client_config),
                 *p_offset + 
                 HEALTH_THERMO_NVM_TEMP_CLIENT_CONFIG_OFFSET);

    }

    /* Increment the offset by the number of words of NVM memory required 
     * by Health Thermometer service 
     */
    *p_offset += HEALTH_THERMO_SERVICE_NVM_MEMORY_WORDS;

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the health 
 *      thermometer service
 *
 *  RETURNS
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *---------------------------------------------------------------------------*/

extern bool HealthThermoCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_HEALTH_THERMOMETER_SERVICE) &&
            (handle <= HANDLE_HEALTH_THERMOMETER_SERVICE_END))
            ? TRUE : FALSE;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoBondingNotify
 *
 *  DESCRIPTION
 *      This function is used by application to notify bonding status to 
 *      health themometer service
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HealthThermoBondingNotify(void)
{

    /* Write data to NVM if bond is established */
    if(AppIsDeviceBonded())
    {
        /* Write to NVM the client configuration value of temperature */
        Nvm_Write((uint16*)&g_ht_serv_data.temp_client_config,
                  sizeof(g_ht_serv_data.temp_client_config),
                  g_ht_serv_data.nvm_offset + 
                  HEALTH_THERMO_NVM_TEMP_CLIENT_CONFIG_OFFSET);
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HealthThermoMeasIndConfigStatus
 *
 *  DESCRIPTION
 *      This function returns whether indications are configured for 
 *      Temperature Measurement characteristic
 *
 *  RETURNS
 *      Boolean - TRUE : Indications are configured for Temperature Measurement 
 *                       characteristic
 *                FALSE: Indications are not configured for Temperature 
 *                       Measurement characteristic
 *
 *---------------------------------------------------------------------------

extern bool HealthThermoMeasIndConfigStatus(void)
{

    return (g_ht_serv_data.temp_client_config & gatt_client_config_indication);

}

*/
#ifdef NVM_TYPE_FLASH
/*----------------------------------------------------------------------------*
 *  NAME
 *      WriteHealthThermometerServiceDataInNvm
 *
 *  DESCRIPTION
 *      This function writes Health Thermometer service data in NVM
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/

extern void WriteHealthThermometerServiceDataInNvm(void)
{
    /* Write to NVM the client configuration value of temperature */
    Nvm_Write((uint16*)&g_ht_serv_data.temp_client_config,
                  sizeof(g_ht_serv_data.temp_client_config),
                  g_ht_serv_data.nvm_offset + 
                  HEALTH_THERMO_NVM_TEMP_CLIENT_CONFIG_OFFSET);
}
#endif /* NVM_TYPE_FLASH */