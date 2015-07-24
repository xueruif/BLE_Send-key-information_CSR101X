/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      health_thermo_uuids.h
 *
 *  DESCRIPTION
 *      UUID MACROs for Health Thermometer service
 *
 *****************************************************************************/

#ifndef __HEALTH_THERMO_UUIDS_H__
#define __HEALTH_THERMO_UUIDS_H__

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser 
 * which creates .c and .h files from .db file doesn't understand
 * brackets and will raise syntax errors.
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/services/Pages/
 * ServiceViewer.aspx?u=org.bluetooth.service.health_thermometer.xml
 */

#define UUID_HEALTH_THERMOMETER_SERVICE            0x1809

#define UUID_HT_TEMPERATURE_MEAS                   0x2a1c

#define UUID_HT_TEMPERATURE_TYPE                   0x2a1d

#define UUID_HT_INTERMEDIATE_TEMP                  0x2a1e

/* Temperature Type Values */
#define HT_TEMP_TYPE_ARMPIT                        0x01
#define HT_TEMP_TYPE_BODY                          0x02
#define HT_TEMP_TYPE_EAR                           0x03
#define HT_TEMP_TYPE_FINGER                        0x04
#define HT_TEMP_TYPE_GASTRO                        0x05
#define HT_TEMP_TYPE_MOUTH                         0x06
#define HT_TEMP_TYPE_RECTUM                        0x07
#define HT_TEMP_TYPE_TOE                           0x08
#define HT_TEMP_TYPE_EAR_DRUM                      0x09


#endif /* __HEALTH_THERMO_UUIDS_H__ */

