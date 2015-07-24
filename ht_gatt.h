/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      ht_gatt.h
 *
 *  DESCRIPTION
 *      Header file for Health Thermometer GATT-related routines
 *
 *****************************************************************************/

#ifndef __HT_GATT_H__
#define __HT_GATT_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <time.h>
#include <gatt_prim.h>

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Value for which advertisement timer needs to be started. 
 *
 * For bonded devices, the timer is initially started for 30 seconds to 
 * enable fast connection by bonded device to the sensor. This is then 
 * followed by reduced power advertisements.
 *
 * For non-bonded devices, the timer is initially started for 30 seconds 
 * to enable fast connections from any collector device in the vicinity.
 * This is then followed by reduced power advertisements.
 */

#define FAST_CONNECTION_ADVERT_TIMEOUT_VALUE     (30 * SECOND)
#define SLOW_CONNECTION_ADVERT_TIMEOUT_VALUE     (1 * MINUTE)

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function handles read operation on attributes (as received in 
 * GATT_ACCESS_IND message) maintained by the application
 */
extern void HandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles Write operation on attributes (as received in 
 * GATT_ACCESS_IND message) maintained by the application.
 */
extern void HandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function is used to start undirected advertisements and moves to 
 * ADVERTISING state
 */
extern void GattStartAdverts(bool fast_connection);

/* This function is used to stop on-going advertisements */
extern void GattStopAdverts(void);

/* This function prepares the list of supported 16-bit service UUIDs to be 
 * added to Advertisement data
 */
extern uint16 GetSupported16BitUUIDServiceList(uint8 *p_service_uuid_ad);

/* This function checks if the address is resolvable random or not */
extern bool GattIsAddressResolvableRandom(TYPED_BD_ADDR_T *p_addr);

#endif /* __HT_GATT_H__ */

