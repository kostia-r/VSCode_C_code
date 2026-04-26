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

/**
 * @brief Describes one target device profile exposed to VM code.
 */
typedef struct MpnDevProfile_t
{
  const char *name;       /**< Human-readable profile name used by host-side profile selection. */
  uint16_t screen_width;  /**< Display width in pixels. */
  uint16_t screen_height; /**< Display height in pixels. */
  uint16_t color_mode;    /**< Encoded display color mode reported by device capability imports. */
  uint16_t sound_flags;   /**< Encoded sound capability flags reported to guest code. */
  uint16_t system_flags;  /**< Encoded system capability flags reported to guest code. */
  uint32_t device_id;     /**< Stable device identifier reported to the guest runtime. */
} MpnDevProfile_t;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Device.h
 *********************************************************************************************************************/
