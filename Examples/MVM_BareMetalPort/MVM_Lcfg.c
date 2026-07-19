/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Lcfg.c
 *           Module:  MVM_BareMetalPort
 *           Target:  Generic bare metal
 *      Description:  Parent-owned profile, runtime pool, and driver bindings for the reference port.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Cfg.h"
#include "MVM_Port.h"

/**********************************************************************************************************************
 *  LOCAL MACROS
 *********************************************************************************************************************/

#define MVM_PORT_RUNTIME_POOL_SIZE                              (1024U * 1024U)

/**********************************************************************************************************************
 *  LOCAL DATA
 *********************************************************************************************************************/

static uint8_t MVM_lRuntimePool[MVM_PORT_RUNTIME_POOL_SIZE];

static const MpnDevProfile_t MVM_lDeviceProfile =
{
  .name = "BOARD_128X160",
  .screen_width = 128U,
  .screen_height = 160U,
  .color_mode = 0x0008U,
  .sound_flags = 0x0000U,
  .system_flags = 0x0025U,
  .key_layout = 0x0001U,
  .frame_interval_ms = 16U,
  .device_id = 0x00020003UL,
  .supported_caps = MVM_DEVICE_CAP_VIDEO | MVM_DEVICE_CAP_INPUT | MVM_DEVICE_CAP_SYSTEM
};

/**********************************************************************************************************************
 *  GLOBAL DATA
 *********************************************************************************************************************/

const MVM_Config_t MVM_Config =
{
  .drivers =
  {
    .display_flush = MVM_PortFlushDisplay,
    .input_get_buttons = MVM_PortGetButtons,
    .audio_queue_pcm = MVM_PortQueuePcm,
    .audio_stop = MVM_PortStopAudio,
    .get_ticks_ms = MVM_PortGetTicksMs,
    .get_random = MVM_PortGetRandom,
    .log = MVM_PortLog,
    .event = MVM_PortEvent,
    .image_read = MVM_PortReadImage,
    .system_message = MVM_PortSystemMessage
  },
  .device_profiles = &MVM_lDeviceProfile,
  .device_profile_count = 1U,
  .device_profile = &MVM_lDeviceProfile,
  .runtime_pool = MVM_lRuntimePool,
  .runtime_pool_size = sizeof(MVM_lRuntimePool)
};

/**********************************************************************************************************************
 *  END OF FILE MVM_Lcfg.c
 *********************************************************************************************************************/
