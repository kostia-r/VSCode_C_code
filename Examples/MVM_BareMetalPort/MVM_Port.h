/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Port.h
 *           Module:  MVM_BareMetalPort
 *           Target:  Generic bare metal
 *      Description:  Board-driver skeleton used by the no-SDL reference port.
 *********************************************************************************************************************/

#ifndef MVM_PORT_H
#define MVM_PORT_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM.h"

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/** @brief Describes one memory-mapped or flash-backed game image. */
typedef struct MVM_PortImage_t
{
  const uint8_t *bytes; /**< Read-only image start. */
  size_t size;          /**< Complete image size in bytes. */
} MVM_PortImage_t;

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

int MVM_PortReadImage(void *user, size_t offset, void *dst, size_t size);
int MVM_PortFlushDisplay(void *user, const MVM_Framebuffer_t *framebuffer);
uint32_t MVM_PortGetButtons(void *user);
int MVM_PortQueuePcm(void *user, const MVM_PcmBuffer_t *pcm);
void MVM_PortStopAudio(void *user);
uint32_t MVM_PortGetTicksMs(void *user);
uint32_t MVM_PortGetRandom(void *user);
int MVM_PortLog(void *user,
                MVM_LogLevel_t level,
                const char *module,
                const char *event,
                const char *message);
void MVM_PortEvent(void *user, MVM_Event_t event, uint32_t arg0, uint32_t arg1);
uint32_t MVM_PortSystemMessage(void *user, uint32_t flags, const char *title, const char *message);
void MVM_PortIdle(void);

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Port.h
 *********************************************************************************************************************/
