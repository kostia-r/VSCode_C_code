/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Port.c
 *           Module:  MVM_BareMetalPort
 *           Target:  Generic bare metal
 *      Description:  Replaceable board-driver skeleton with safe no-op implementations.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Port.h"
#include <string.h>

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

int MVM_PortReadImage(void *user, size_t offset, void *dst, size_t size)
{
  const MVM_PortImage_t *image = (const MVM_PortImage_t *)user;

  if (!image || !image->bytes || !dst || offset > image->size || size > image->size - offset)
  {
    return -1;
  }

  memcpy(dst, image->bytes + offset, size);
  return 0;
} /* End of MVM_PortReadImage */

int MVM_PortFlushDisplay(void *user, const MVM_Framebuffer_t *framebuffer)
{
  (void)user;

  if (!framebuffer || !framebuffer->pixels || framebuffer->pixel_format != MVM_PIXEL_FORMAT_RGB565)
  {
    return -1;
  }

  /* Replace with a non-blocking LCD/DMA transfer of framebuffer->dirty_rect. */
  return 0;
} /* End of MVM_PortFlushDisplay */

uint32_t MVM_PortGetButtons(void *user)
{
  (void)user;
  /* Translate GPIO/keypad state to MVM_BUTTON_*_MASK values. */
  return 0U;
} /* End of MVM_PortGetButtons */

int MVM_PortQueuePcm(void *user, const MVM_PcmBuffer_t *pcm)
{
  (void)user;
  (void)pcm;
  return 0;
} /* End of MVM_PortQueuePcm */

void MVM_PortStopAudio(void *user)
{
  (void)user;
} /* End of MVM_PortStopAudio */

uint32_t MVM_PortGetTicksMs(void *user)
{
  static uint32_t ticksMs;

  (void)user;
  /* Replace with one monotonic RTOS/HAL tick converted to milliseconds. */
  return ++ticksMs;
} /* End of MVM_PortGetTicksMs */

uint32_t MVM_PortGetRandom(void *user)
{
  (void)user;
  /* A constant is valid for bring-up; replace with a board RNG when available. */
  return 1U;
} /* End of MVM_PortGetRandom */

int MVM_PortLog(void *user,
                MVM_LogLevel_t level,
                const char *module,
                const char *event,
                const char *message)
{
  (void)user;
  (void)level;
  (void)module;
  (void)event;
  (void)message;
  return 0;
} /* End of MVM_PortLog */

void MVM_PortEvent(void *user, MVM_Event_t event, uint32_t arg0, uint32_t arg1)
{
  (void)user;
  (void)event;
  (void)arg0;
  (void)arg1;
} /* End of MVM_PortEvent */

uint32_t MVM_PortSystemMessage(void *user, uint32_t flags, const char *title, const char *message)
{
  (void)user;
  (void)flags;
  (void)title;
  (void)message;
  return 1U;
} /* End of MVM_PortSystemMessage */

void MVM_PortIdle(void)
{
  /* Replace with __WFI(), taskYIELD(), or the platform scheduler hook. */
} /* End of MVM_PortIdle */
