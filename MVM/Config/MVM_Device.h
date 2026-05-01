/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Device.h
 *           Module:  MVM_Config
 *           Target:  Portable C
 *      Description:  Internal device profile definitions used by the integration configuration layer.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_DEVICE_H
#define MVM_DEVICE_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

#define MVM_DEVICE_CAP_VIDEO                                    (1UL << 0U)
#define MVM_DEVICE_CAP_INPUT                                    (1UL << 1U)
#define MVM_DEVICE_CAP_SOUND                                    (1UL << 2U)
#define MVM_DEVICE_CAP_COMM                                     (1UL << 3U)
#define MVM_DEVICE_CAP_SYSTEM                                   (1UL << 4U)

/**
 * @brief Describes one target device profile exposed to VM code.
 */
typedef struct MpnDevProfile_t
{
  const char *name;             /**< Human-readable profile name used by host-side profile selection. */
  uint16_t screen_width;        /**< Display width in pixels. */
  uint16_t screen_height;       /**< Display height in pixels. */
  uint16_t color_mode;          /**< Encoded display color mode reported by device capability imports. */
  uint16_t sound_flags;         /**< Encoded sound capability flags reported to guest code. */
  uint16_t system_flags;        /**< Encoded system capability flags reported to guest code. */
  uint16_t key_layout;          /**< Encoded key-layout identifier used by input-facing integration. */
  uint16_t frame_interval_ms;   /**< Default frame/tick interval used by timing fallbacks. */
  uint32_t device_id;           /**< Stable guest-visible device identifier. */
  uint32_t memory_limit_bytes;  /**< Reported or assumed device-side working-memory limit. */
  uint32_t supported_caps;      /**< Bit-mask of capability queries supported by this profile. */
} MpnDevProfile_t;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Device.h
 *********************************************************************************************************************/
