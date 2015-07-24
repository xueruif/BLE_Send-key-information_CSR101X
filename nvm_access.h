/******************************************************************************
 *  Copyright Cambridge Silicon Radio Limited 2012-2014
 *  Part of CSR uEnergy SDK 2.3.0
 *  Application version 2.3.0.0
 *
 *  FILE
 *      nvm_access.h
 *
 *  DESCRIPTION
 *      Header definitions for NVM usage.
 *
 ******************************************************************************/

#ifndef __NVM_ACCESS_H__
#define __NVM_ACCESS_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <status.h>

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function is used to perform things necessary to save power on NVM 
 * once the read/write operations are done
 */
extern void Nvm_Disable(void);

/* Read words from the NVM store after preparing the NVM to be readable */
extern void Nvm_Read(uint16* buffer, uint16 length, uint16 offset);

/* Write words to the NVM store after preparing the NVM to be writable */
extern void Nvm_Write(uint16* buffer, uint16 length, uint16 offset);

#ifdef NVM_TYPE_FLASH
/* Erases the NVM memory.*/
extern void Nvm_Erase(void);
#endif /* NVM_TYPE_FLASH */

#endif /* __NVM_ACCESS_H__ */