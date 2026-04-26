/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Lcfg.c
 *           Module:  MVM_Config
 *           Target:  Portable C
 *      Description:  Default integration configuration, runtime pool, device profiles, and platform bindings for the VM.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_BuildCfg.h"
#include "MVM_Cfg.h"
#include "MVM_Internal.h"
#include <limits.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <sys/types.h>
#endif

/**********************************************************************************************************************
 *  LOCAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Stores the default runtime pool for the built-in integration config.
 */
typedef struct MVM_DefRuntimePool_t
{
  uint8_t au8Memory[MVM_CFG_RUNTIME_POOL_SIZE]; /**< Static VM runtime arena storage. */
} MVM_DefRuntimePool_t;

/**********************************************************************************************************************
 *  LOCAL FUNCTION PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Reads one byte range from one file-backed image source.
 */
static int MVM_lReadFileImage(void *user, size_t offset, void *dst, size_t size);

/**********************************************************************************************************************
 *  LOCAL DATA
 *********************************************************************************************************************/

static MVM_DefRuntimePool_t MVM_lDefRuntimePool;

static const MpnDevProfile_t MVM_lDevProfiles[] =
{
  {
    /* Human-readable profile identifier used by the host integration. */
    .name = "SE_T310",

    /* Visible framebuffer width reported to vGetCaps. */
    .screen_width = 181U,

    /* Visible framebuffer height reported to vGetCaps. */
    .screen_height = 101U,

    /* Encoded display capability flags reported to the guest. */
    .color_mode = 0x000FU,

    /* Encoded audio capability flags reported to the guest. */
    .sound_flags = 0x00A7U,

    /* Encoded system capability flags reported to the guest. */
    .system_flags = 0x0025U,

    /* Encoded keypad/layout identifier used by input-facing integration. */
    .key_layout = 0x0001U,

    /* Default frame interval used by timing fallbacks. */
    .frame_interval_ms = 16U,

    /* Device identifier reported through the capability query API. */
    .device_id = (((uint32_t)1U << 16) | 3U),

    /* Assumed working-memory limit for profile-level policy and future save checks. */
    .memory_limit_bytes = 0u,

    /* Capability queries supported by this profile. */
    .supported_caps = MVM_DEVICE_CAP_VIDEO | MVM_DEVICE_CAP_COLOR | MVM_DEVICE_CAP_SOUND | MVM_DEVICE_CAP_SYSTEM,
  },
  {
    /* Human-readable profile identifier used by the host integration. */
    .name = "SE_T610",

    /* Visible framebuffer width reported to vGetCaps. */
    .screen_width = 128U,

    /* Visible framebuffer height reported to vGetCaps. */
    .screen_height = 160U,

    /* Encoded display capability flags reported to the guest. */
    .color_mode = 0x000FU,

    /* Encoded audio capability flags reported to the guest. */
    .sound_flags = 0x00A7U,

    /* Encoded system capability flags reported to the guest. */
    .system_flags = 0x0025U,

    /* Encoded keypad/layout identifier used by input-facing integration. */
    .key_layout = 0x0001U,

    /* Default frame interval used by timing fallbacks. */
    .frame_interval_ms = 16U,

    /* Device identifier reported through the capability query API. */
    .device_id = (((uint32_t)2U << 16) | 3U),

    /* Assumed working-memory limit for profile-level policy and future save checks. */
    .memory_limit_bytes = 0u,

    /* Capability queries supported by this profile. */
    .supported_caps = MVM_DEVICE_CAP_VIDEO | MVM_DEVICE_CAP_COLOR | MVM_DEVICE_CAP_SOUND | MVM_DEVICE_CAP_SYSTEM,
  }
};

/**********************************************************************************************************************
 *  GLOBAL DATA
 *********************************************************************************************************************/

const MVM_Config_t MVM_Config =
{
  .platform =
  {
    /* Opaque user context passed to get_ticks_ms/get_random/log. */
    .user = NULL,

    /* Host tick callback expected to return milliseconds. */
    .get_ticks_ms = NULL,

    /* Host random callback used by the guest random import path. */
    .get_random = NULL,

    /* Host logger callback used by trace/debug output. */
#if MVM_ENABLE_DEFAULT_LOGGER
    .log = MVM_DefaultLog,
#else
    .log = NULL,
#endif
    .event = NULL
  },

  /* Compile-time image backend used to read the selected VM *.mpn file source. */
  .image_read = MVM_lReadFileImage,
  .image_map = NULL,
  .image_unmap = NULL,

  /* Full device-profile catalog exposed by this integration. */
  .device_profiles = MVM_lDevProfiles,

  /* Number of entries available in the device-profile catalog. */
  .device_profile_count = (uint32_t)(sizeof(MVM_lDevProfiles) / sizeof(MVM_lDevProfiles[0])),

  /* Device profile returned through platform-owned vGetCaps queries. */
  .device_profile = &MVM_lDevProfiles[0],

  /* Host may still inject additional syscalls later; the built-in imports live in MVM_Imports.c. */
  .syscalls = NULL,

  /* No built-in config-side syscall table remains. */
  .syscall_count = 0u,

  /* Static arena shared by guest RAM and VM-owned loader metadata. */
  .runtime_pool = MVM_lDefRuntimePool.au8Memory,

  /* Total number of bytes available in the static runtime arena. */
  .runtime_pool_size = sizeof(MVM_lDefRuntimePool.au8Memory),

  /* Default no-progress step budget. Zero keeps the watchdog disabled. */
  .watchdog_limit = MVM_CFG_DEFAULT_WATCHDOG_LIMIT
};

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_lReadFileImage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Reads one byte range from one file-backed image source.
 *********************************************************************************************************************/
static int MVM_lReadFileImage(void *user, size_t offset, void *dst, size_t size)
{
  FILE *file = (FILE *)user;

  if (!file || !dst)
  {
    return -1;
  }

#if defined(_WIN32)
  if (_fseeki64(file, (__int64)offset, SEEK_SET) != 0)
#elif defined(_LARGEFILE_SOURCE) || defined(_LARGEFILE64_SOURCE) || defined(_FILE_OFFSET_BITS)
  if (fseeko(file, (off_t)offset, SEEK_SET) != 0)
#else
  if (offset > (size_t)LONG_MAX || fseek(file, (long)offset, SEEK_SET) != 0)
#endif
  {
    return -1;
  }

  if (fread(dst, 1u, size, file) != size)
  {
    return -1;
  }

  return 0;
} /* End of MVM_lReadFileImage */

/**********************************************************************************************************************
 *  END OF FILE MVM_Lcfg.c
 *********************************************************************************************************************/
