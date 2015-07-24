/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      health_thermo_service.h
 *
 *  DESCRIPTION
 *      Header definitions for Health Thermometer service
 *
 *****************************************************************************/

#ifndef __HEALTH_THERMO_SERVICE_H__
#define __HEALTH_THERMO_SERVICE_H__

/*============================================================================*
 *  SDK Header Files
 *===========================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*============================================================================*
 *  Public Function Prototypes
 *===========================================================================*/

/* This function is used to initialise Health Thermometer service data 
 * structure
 */
extern void HealthThermoDataInit(void);

/* This function handles read operation on health thermometer service 
 * attributes maintained by the application
 */
extern void HealthThermoHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles write operation on health thermometer service 
 * attributes maintained by the application
 */
extern void HealthThermoHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function is used to send temperature reading as an indication 
 * to the connected host
 */
extern bool HealthThermoSendTempReading(uint16 ucid, uint8 *value);

/* This function is used to set the status of pending confirmation for the 
 * transmitted temperature measurement indications
 
extern void HealthThermoRegIndicationCfm(bool ind_state);
*/
/* This function is used to read health thermometer service specific data 
 * stored in NVM
 */
extern void HealthThermoReadDataFromNVM(uint16 *p_offset);

/* This function is used to check if the handle belongs to the health 
 * thermometer service
 */
extern bool HealthThermoCheckHandleRange(uint16 handle);

/* This function is used by application to notify bonding status to 
 * health themometer service
 */
extern void HealthThermoBondingNotify(void);

/* This function returns whether indications are configured for Temperature
 * Measurement characteristic

extern bool HealthThermoMeasIndConfigStatus(void);
 */
#ifdef NVM_TYPE_FLASH
/* This function writes Health Thermometer service data in NVM */
extern void WriteHealthThermometerServiceDataInNvm(void);
#endif /* NVM_TYPE_FLASH */

#endif /* __HEALTH_THERMO_SERVICE_H__ */
