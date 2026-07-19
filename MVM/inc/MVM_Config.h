/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Config.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public integration configuration contract for parent projects and platform ports.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_CONFIG_H
#define MVM_CONFIG_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Device.h"
#include "MVM_Drivers.h"
#include "MVM_Types.h"

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Describes one complete parent-owned integration instance for the VM.
 */
typedef struct MVM_Config_t
{
  MpnPlatform_t platform;                    /**< Host callback table used by the VM core. */
  MVM_Drivers_t drivers;                     /**< Hardware-oriented callbacks used when legacy hooks are absent. */
  MpnImageReadFn_t image_read;               /**< Image-backend range-read callback used by the VM core. */
  MpnImageWriteFn_t image_write;             /**< Optional image range-write callback for persistent data. */
  MpnImageMapFn_t image_map;                 /**< Optional image-backend map callback used by the VM core. */
  MpnImageUnmapFn_t image_unmap;             /**< Optional image-backend unmap callback used by the VM core. */
  const MpnDevProfile_t *device_profiles;    /**< Catalog of device profiles offered by this integration. */
  uint32_t device_profile_count;             /**< Number of entries in the device profile catalog. */
  const MpnDevProfile_t *device_profile;     /**< Active device profile exposed to guest imports. */
  const MpnSyscall_t *syscalls;              /**< Optional host syscall table visible to the dispatcher. */
  uint32_t syscall_count;                    /**< Number of entries in the host syscall table. */
  void *runtime_pool;                        /**< Backing arena used for VM state, RAM, and decoded metadata. */
  size_t runtime_pool_size;                  /**< Total size of the runtime arena in bytes. */
  uint32_t watchdog_limit;                   /**< Default no-progress watchdog budget in VM steps. */
} MVM_Config_t;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Config.h
 *********************************************************************************************************************/
