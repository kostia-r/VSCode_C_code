/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Cfg.h
 *           Module:  MVM_Config
 *           Target:  Portable C
 *      Description:  Internal integration-time VM configuration macros, callback adapters, and built-in config object.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_CFG_H
#define MVM_CFG_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Types.h"
#include "MVM_Device.h"
#include <string.h>

/**********************************************************************************************************************
 *  LOCAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Adapts a no-argument host tick source to MpnPlatform_t.get_ticks_ms.
 */
typedef struct MVM_TicksNoArgAdapter_t
{
  uint32_t (*get_ticks)(void); /**< Host tick source with no user argument. */
  uint32_t ticks_per_ms;       /**< Conversion factor from host ticks to milliseconds. */
} MVM_TicksNoArgAdapter_t;

/**
 * @brief Adapts a host tick source with its own user argument to get_ticks_ms.
 */
typedef struct MVM_TicksArgAdapter_t
{
  uint32_t (*get_ticks)(void *arg); /**< Host tick source using a foreign user pointer. */
  void *arg;                        /**< Foreign user pointer passed to the host tick source. */
  uint32_t ticks_per_ms;            /**< Conversion factor from host ticks to milliseconds. */
} MVM_TicksArgAdapter_t;

/**
 * @brief Adapts a no-argument host random source to MpnPlatform_t.get_random.
 */
typedef struct MVM_RandomNoArgAdapter_t
{
  uint32_t (*get_random)(void); /**< Host random source with no user argument. */
} MVM_RandomNoArgAdapter_t;

/**
 * @brief Adapts a foreign-argument host random source to MpnPlatform_t.get_random.
 */
typedef struct MVM_RandomArgAdapter_t
{
  uint32_t (*get_random)(void *arg); /**< Host random source using a foreign user pointer. */
  void *arg;                         /**< Foreign user pointer passed to the host random source. */
} MVM_RandomArgAdapter_t;

/**
 * @brief Adapts a no-argument host logger to MpnPlatform_t.log.
 */
typedef struct MVM_LogNoArgAdapter_t
{
  void (*log)(MVM_LogLevel_t level,
              const char *module,
              const char *event,
              const char *message); /**< Host logger with no user argument and no return code. */
} MVM_LogNoArgAdapter_t;

/**
 * @brief Adapts a foreign-argument host logger to MpnPlatform_t.log.
 */
typedef struct MVM_LogArgAdapter_t
{
  void (*log)(void *arg,
              MVM_LogLevel_t level,
              const char *module,
              const char *event,
              const char *message);            /**< Host logger using a foreign user pointer. */
  void *arg;                                   /**< Foreign user pointer passed to the host logger. */
} MVM_LogArgAdapter_t;

/**
 * @brief Describes one complete internal integration instance for the VM.
 */
typedef struct MVM_Config_t
{
  MpnPlatform_t platform;                    /**< Host callback table used by the VM core. */
  MpnImageReadFn_t image_read;               /**< Image-backend range-read callback used by the VM core. */
  MpnImageMapFn_t image_map;                 /**< Optional image-backend map callback used by the VM core. */
  MpnImageUnmapFn_t image_unmap;             /**< Optional image-backend unmap callback used by the VM core. */
  const MpnDevProfile_t *device_profiles;    /**< Catalog of device profiles offered by this integration. */
  uint32_t device_profile_count;             /**< Number of entries in the device profile catalog. */
  const MpnDevProfile_t *device_profile;     /**< Active device profile exposed to guest imports. */
  const MpnSyscall_t *syscalls;              /**< Host syscall table visible to the runtime dispatcher. */
  uint32_t syscall_count;                    /**< Number of entries in the syscall table. */
  void *runtime_pool;                        /**< Backing arena used for VM state, RAM, and decoded metadata. */
  size_t runtime_pool_size;                  /**< Total size of the runtime arena in bytes. */
  uint32_t watchdog_limit;                   /**< Default no-progress watchdog budget in VM steps. */
} MVM_Config_t;

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

/* Total size of the static runtime arena owned by the built-in integration config. */
#ifndef MVM_CFG_RUNTIME_POOL_SIZE
#define MVM_CFG_RUNTIME_POOL_SIZE                               (1024U * 1024U)
#endif

/* When enabled, VM init buffers are allocated from the host heap instead of the static runtime arena. */
#ifndef MVM_CFG_USE_HOST_HEAP
#define MVM_CFG_USE_HOST_HEAP                                   (0U)
#endif

/* Default soft-watchdog budget. Zero keeps the watchdog disabled. */
#ifndef MVM_CFG_DEFAULT_WATCHDOG_LIMIT
#define MVM_CFG_DEFAULT_WATCHDOG_LIMIT                          (0U)
#endif

/* Short logger context label used by the built-in default logger. */
#ifndef MVM_CFG_LOG_CONTEXT_NAME
#define MVM_CFG_LOG_CONTEXT_NAME                                ("MVM")
#endif

/**********************************************************************************************************************
 *  GLOBAL INLINE FUNCTIONS
 *********************************************************************************************************************/

/**
 * @brief Adapts a no-argument host tick source to MpnPlatform_t.get_ticks_ms.
 */
static inline uint32_t MVM_Cfg_lAdaptTicksNoArg(void *user)
{
  const MVM_TicksNoArgAdapter_t *adapter = (const MVM_TicksNoArgAdapter_t *)user;
  uint32_t ticks = 0u;

  if (!adapter || !adapter->get_ticks)
  {
    return 0u;
  }

  ticks = adapter->get_ticks();

  if (adapter->ticks_per_ms > 1u)
  {
    ticks /= adapter->ticks_per_ms;
  }

  return ticks;
}

/**
 * @brief Adapts a foreign-argument host tick source to MpnPlatform_t.get_ticks_ms.
 */
static inline uint32_t MVM_Cfg_lAdaptTicksArg(void *user)
{
  const MVM_TicksArgAdapter_t *adapter = (const MVM_TicksArgAdapter_t *)user;
  uint32_t ticks = 0u;

  if (!adapter || !adapter->get_ticks)
  {
    return 0u;
  }

  ticks = adapter->get_ticks(adapter->arg);

  if (adapter->ticks_per_ms > 1u)
  {
    ticks /= adapter->ticks_per_ms;
  }

  return ticks;
}

/**
 * @brief Adapts a no-argument host random source to MpnPlatform_t.get_random.
 */
static inline uint32_t MVM_Cfg_lAdaptRandomNoArg(void *user)
{
  const MVM_RandomNoArgAdapter_t *adapter = (const MVM_RandomNoArgAdapter_t *)user;

  if (!adapter || !adapter->get_random)
  {
    return 0u;
  }

  return adapter->get_random();
}

/**
 * @brief Adapts a foreign-argument host random source to MpnPlatform_t.get_random.
 */
static inline uint32_t MVM_Cfg_lAdaptRandomArg(void *user)
{
  const MVM_RandomArgAdapter_t *adapter = (const MVM_RandomArgAdapter_t *)user;

  if (!adapter || !adapter->get_random)
  {
    return 0u;
  }

  return adapter->get_random(adapter->arg);
}

/**
 * @brief Adapts a no-argument host logger to MpnPlatform_t.log.
 */
static inline int MVM_Cfg_lAdaptLogNoArg(void *user,
                                         MVM_LogLevel_t level,
                                         const char *module,
                                         const char *event,
                                         const char *message)
{
  const MVM_LogNoArgAdapter_t *adapter = (const MVM_LogNoArgAdapter_t *)user;

  if (!adapter || !adapter->log)
  {
    return 0;
  }

  adapter->log(level, module, event, message);

  return 0;
}

/**
 * @brief Adapts a foreign-argument host logger to MpnPlatform_t.log.
 */
static inline int MVM_Cfg_lAdaptLogArg(void *user,
                                       MVM_LogLevel_t level,
                                       const char *module,
                                       const char *event,
                                       const char *message)
{
  const MVM_LogArgAdapter_t *adapter = (const MVM_LogArgAdapter_t *)user;

  if (!adapter || !adapter->log)
  {
    return 0;
  }

  adapter->log(adapter->arg, level, module, event, message);

  return 0;
}

/**
 * @brief Finds one named device profile inside one integration config.
 */
static inline const MpnDevProfile_t *MVM_Cfg_lFindDevProfileByName(const MVM_Config_t *config, const char *name)
{
  uint32_t i = 0u;

  if (!config || !name || !config->device_profiles)
  {
    return NULL;
  }

  for (i = 0u; i < config->device_profile_count; ++i)
  {
    if (config->device_profiles[i].name && strcmp(config->device_profiles[i].name, name) == 0)
    {
      return &config->device_profiles[i];
    }
  }

  return NULL;
}

/**********************************************************************************************************************
 *  GLOBAL DATA PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Built-in integration object used internally by MVM_Init().
 */
extern const MVM_Config_t MVM_Config;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Cfg.h
 *********************************************************************************************************************/
