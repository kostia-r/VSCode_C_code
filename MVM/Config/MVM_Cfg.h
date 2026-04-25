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
 * @brief Adapts a no-argument host tick source to MophunPlatform.get_ticks_ms.
 */
typedef struct MVM_tstTicksNoArgAdapter
{
  uint32_t (*get_ticks)(void); /**< Host tick source with no user argument. */
  uint32_t ticks_per_ms;       /**< Conversion factor from host ticks to milliseconds. */
} MVM_tstTicksNoArgAdapter;

/**
 * @brief Adapts a host tick source with its own user argument to get_ticks_ms.
 */
typedef struct MVM_tstTicksArgAdapter
{
  uint32_t (*get_ticks)(void *arg); /**< Host tick source using a foreign user pointer. */
  void *arg;                        /**< Foreign user pointer passed to the host tick source. */
  uint32_t ticks_per_ms;            /**< Conversion factor from host ticks to milliseconds. */
} MVM_tstTicksArgAdapter;

/**
 * @brief Adapts a no-argument host random source to MophunPlatform.get_random.
 */
typedef struct MVM_tstRandomNoArgAdapter
{
  uint32_t (*get_random)(void); /**< Host random source with no user argument. */
} MVM_tstRandomNoArgAdapter;

/**
 * @brief Adapts a foreign-argument host random source to MophunPlatform.get_random.
 */
typedef struct MVM_tstRandomArgAdapter
{
  uint32_t (*get_random)(void *arg); /**< Host random source using a foreign user pointer. */
  void *arg;                         /**< Foreign user pointer passed to the host random source. */
} MVM_tstRandomArgAdapter;

/**
 * @brief Adapts a no-argument host logger to MophunPlatform.log.
 */
typedef struct MVM_tstLogNoArgAdapter
{
  void (*log)(const char *message); /**< Host logger with no user argument and no return code. */
} MVM_tstLogNoArgAdapter;

/**
 * @brief Adapts a foreign-argument host logger to MophunPlatform.log.
 */
typedef struct MVM_tstLogArgAdapter
{
  void (*log)(void *arg, const char *message); /**< Host logger using a foreign user pointer. */
  void *arg;                                   /**< Foreign user pointer passed to the host logger. */
} MVM_tstLogArgAdapter;

/**
 * @brief Describes one complete internal integration instance for the VM.
 */
typedef struct MVM_tstConfig
{
  MophunPlatform platform;                    /**< Host callback table used by the VM core. */
  const MophunDeviceProfile *device_profiles; /**< Catalog of device profiles offered by this integration. */
  uint32_t device_profile_count;              /**< Number of entries in the device profile catalog. */
  const MophunDeviceProfile *device_profile;  /**< Active device profile exposed to guest imports. */
  const MophunSyscall *syscalls;              /**< Host syscall table visible to the runtime dispatcher. */
  uint32_t syscall_count;                     /**< Number of entries in the syscall table. */
  void *runtime_pool;                         /**< Backing arena used for VM state, RAM, and decoded metadata. */
  size_t runtime_pool_size;                   /**< Total size of the runtime arena in bytes. */
  uint32_t watchdog_limit;                    /**< Default no-progress watchdog budget in VM steps. */
} MVM_tstConfig;

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

/* Total size of the static runtime arena owned by the built-in integration config. */
#ifndef MVM_CFG_RUNTIME_POOL_SIZE
#define MVM_CFG_RUNTIME_POOL_SIZE                               (1024U * 1024U)
#endif

/* Default soft-watchdog budget. Zero keeps the watchdog disabled. */
#ifndef MVM_CFG_DEFAULT_WATCHDOG_LIMIT
#define MVM_CFG_DEFAULT_WATCHDOG_LIMIT                          (0U)
#endif

/* Device profile screen width reported through vGetCaps. */
#ifndef MVM_CFG_DEVICE_SCREEN_WIDTH
#define MVM_CFG_DEVICE_SCREEN_WIDTH                             (101U)
#endif

/* Device profile screen height reported through vGetCaps. */
#ifndef MVM_CFG_DEVICE_SCREEN_HEIGHT
#define MVM_CFG_DEVICE_SCREEN_HEIGHT                            (80U)
#endif

/* Encoded device color capability flags reported through vGetCaps. */
#ifndef MVM_CFG_DEVICE_COLOR_MODE
#define MVM_CFG_DEVICE_COLOR_MODE                               (0x000FU)
#endif

/* Encoded device sound capability flags reported through vGetCaps. */
#ifndef MVM_CFG_DEVICE_SOUND_FLAGS
#define MVM_CFG_DEVICE_SOUND_FLAGS                              (0x00A7U)
#endif

/* Encoded system capability flags reported through vGetCaps. */
#ifndef MVM_CFG_DEVICE_SYSTEM_FLAGS
#define MVM_CFG_DEVICE_SYSTEM_FLAGS                             (0x0025U)
#endif

/* Encoded device identifier reported through vGetCaps. */
#ifndef MVM_CFG_DEVICE_ID
#define MVM_CFG_DEVICE_ID                                       (((uint32_t)1U << 16) | 3U)
#endif

/**********************************************************************************************************************
 *  GLOBAL INLINE FUNCTIONS
 *********************************************************************************************************************/

/**
 * @brief Adapts a no-argument host tick source to MophunPlatform.get_ticks_ms.
 */
static inline uint32_t MVM_Cfg_u32AdaptTicksNoArg(void *user)
{
  const MVM_tstTicksNoArgAdapter *adapter = (const MVM_tstTicksNoArgAdapter *)user;
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
 * @brief Adapts a foreign-argument host tick source to MophunPlatform.get_ticks_ms.
 */
static inline uint32_t MVM_Cfg_u32AdaptTicksArg(void *user)
{
  const MVM_tstTicksArgAdapter *adapter = (const MVM_tstTicksArgAdapter *)user;
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
 * @brief Adapts a no-argument host random source to MophunPlatform.get_random.
 */
static inline uint32_t MVM_Cfg_u32AdaptRandomNoArg(void *user)
{
  const MVM_tstRandomNoArgAdapter *adapter = (const MVM_tstRandomNoArgAdapter *)user;

  if (!adapter || !adapter->get_random)
  {
    return 0u;
  }

  return adapter->get_random();
}

/**
 * @brief Adapts a foreign-argument host random source to MophunPlatform.get_random.
 */
static inline uint32_t MVM_Cfg_u32AdaptRandomArg(void *user)
{
  const MVM_tstRandomArgAdapter *adapter = (const MVM_tstRandomArgAdapter *)user;

  if (!adapter || !adapter->get_random)
  {
    return 0u;
  }

  return adapter->get_random(adapter->arg);
}

/**
 * @brief Adapts a no-argument host logger to MophunPlatform.log.
 */
static inline int MVM_Cfg_iAdaptLogNoArg(void *user, const char *message)
{
  const MVM_tstLogNoArgAdapter *adapter = (const MVM_tstLogNoArgAdapter *)user;

  if (!adapter || !adapter->log)
  {
    return 0;
  }

  adapter->log(message);

  return 0;
}

/**
 * @brief Adapts a foreign-argument host logger to MophunPlatform.log.
 */
static inline int MVM_Cfg_iAdaptLogArg(void *user, const char *message)
{
  const MVM_tstLogArgAdapter *adapter = (const MVM_tstLogArgAdapter *)user;

  if (!adapter || !adapter->log)
  {
    return 0;
  }

  adapter->log(adapter->arg, message);

  return 0;
}

/**
 * @brief Finds one named device profile inside one integration config.
 */
static inline const MophunDeviceProfile *MVM_Cfg_pcdtFindDeviceProfileByName(const MVM_tstConfig *config,
                                                                              const char *name)
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
 * @brief Built-in integration object used internally by MVM_enuInit().
 */
extern const MVM_tstConfig MVM_kstConfig;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Cfg.h
 *********************************************************************************************************************/
