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
#include <stdio.h>

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
 * @brief Writes one diagnostic message through the default platform logger.
 */
static int MVM_lDefaultLog(void *user, const char *message);

/**
 * @brief Handles platform-owned imports that complete with a zero result.
 */
static uint32_t MVM_lPlatformRetZero(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned VM termination import.
 */
static uint32_t MVM_lPlatformTerminate(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned tick-count import.
 */
static uint32_t MVM_lPlatformGetTickCount(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned random-seed import.
 */
static uint32_t MVM_lPlatformSetRandom(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned random-value import.
 */
static uint32_t MVM_lPlatformGetRandom(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned capability-query import.
 */
static uint32_t MVM_lPlatformGetCaps(MpnVM_t *vm, void *user);

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

    /* Device identifier reported through the capability query API. */
    .device_id = (((uint32_t)1U << 16) | 3U),
  }
};

static const MpnSyscall_t MVM_lPlatformSyscalls[] =
{
  /* Platform query and control imports implemented by this integration layer. */
  { .name = "vGetCaps", .fn = MVM_lPlatformGetCaps, .user = NULL },
  { .name = "vGetTickCount", .fn = MVM_lPlatformGetTickCount, .user = NULL },
  { .name = "vSetRandom", .fn = MVM_lPlatformSetRandom, .user = NULL },
  { .name = "vGetRandom", .fn = MVM_lPlatformGetRandom, .user = NULL },
  { .name = "vTerminateVMGP", .fn = MVM_lPlatformTerminate, .user = NULL },

  /* Default platform stubs for graphics, audio, UI, and control imports. */
  { .name = "DbgPrintf", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vPrint", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vClearScreen", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vDrawLine", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vDrawObject", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vFillRect", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vFlipScreen", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vGetButtonData", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vGetPaletteEntry", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vMapDispose", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vMapGetAttribute", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vMapInit", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vMapSetTile", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vMapSetXY", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vMsgBox", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vPlayResource", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetActiveFont", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetBackColor", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetClipWindow", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetForeColor", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetPalette", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetPaletteEntry", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSetTransferMode", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSpriteBoxCollision", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSpriteCollision", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSpriteDispose", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSpriteInit", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSpriteSet", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vStreamWrite", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vSysCtl", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vTestKey", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vUpdateMap", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vUpdateSprite", .fn = MVM_lPlatformRetZero, .user = NULL },
  { .name = "vitoa", .fn = MVM_lPlatformRetZero, .user = NULL }
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
    .log = MVM_lDefaultLog
#else
    .log = NULL
#endif
  },

  /* Full device-profile catalog exposed by this integration. */
  .device_profiles = MVM_lDevProfiles,

  /* Number of entries available in the device-profile catalog. */
  .device_profile_count = (uint32_t)(sizeof(MVM_lDevProfiles) / sizeof(MVM_lDevProfiles[0])),

  /* Device profile returned through platform-owned vGetCaps queries. */
  .device_profile = &MVM_lDevProfiles[0],

  /* Syscall table for platform-owned VM imports. */
  .syscalls = MVM_lPlatformSyscalls,

  /* Number of entries visible in the platform syscall table. */
  .syscall_count = (uint32_t)(sizeof(MVM_lPlatformSyscalls) / sizeof(MVM_lPlatformSyscalls[0])),

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
 *  Name: MVM_lDefaultLog
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Writes one diagnostic message through the default platform logger.
 *********************************************************************************************************************/
static int MVM_lDefaultLog(void *user, const char *message)
{
  (void)user;

  return fputs(message, stdout);
} /* End of MVM_lDefaultLog */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformRetZero
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles platform-owned imports that complete with a zero result.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformRetZero(MpnVM_t *vm, void *user)
{
  (void)vm;
  (void)user;

  return 0u;
} /* End of MVM_lPlatformRetZero */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformTerminate
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned VM termination import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformTerminate(MpnVM_t *vm, void *user)
{
  (void)user;

  MVM_RequestExitRaw(vm);

  return 0u;
} /* End of MVM_lPlatformTerminate */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformGetTickCount
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned tick-count import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformGetTickCount(MpnVM_t *vm, void *user)
{
  VMGPContext *ctx = (VMGPContext *)vm;

  (void)user;

  if (!ctx)
  {
    return 0u;
  }

  if (ctx->platform.get_ticks_ms)
  {
    ctx->tick_count = ctx->platform.get_ticks_ms(ctx->platform.user);
  }
  else
  {
    ctx->tick_count += 16u;
  }

  return ctx->tick_count;
} /* End of MVM_lPlatformGetTickCount */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformSetRandom
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned random-seed import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformSetRandom(MpnVM_t *vm, void *user)
{
  VMGPContext *ctx = (VMGPContext *)vm;

  (void)user;

  if (!ctx)
  {
    return 0u;
  }

  ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;

  return 0u;
} /* End of MVM_lPlatformSetRandom */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformGetRandom
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned random-value import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformGetRandom(MpnVM_t *vm, void *user)
{
  VMGPContext *ctx = (VMGPContext *)vm;

  (void)user;

  if (!ctx)
  {
    return 0u;
  }

  if (ctx->platform.get_random)
  {
    return ctx->platform.get_random(ctx->platform.user);
  }

  ctx->random_state = ctx->random_state * 1103515245u + 12345u;

  return (ctx->random_state >> 16) & 0xFFFFu;
} /* End of MVM_lPlatformGetRandom */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformGetCaps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned capability-query import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformGetCaps(MpnVM_t *vm, void *user)
{
  VMGPContext *ctx = (VMGPContext *)vm;
  const MpnDevProfile_t *profile = NULL;
  uint32_t query = 0;
  uint32_t out = 0;
  uint32_t result = 0;

  (void)user;

  if (!ctx || !ctx->device_profile)
  {
    return 0u;
  }

  profile = ctx->device_profile;
  query = ctx->regs[VM_REG_P0];
  out = ctx->regs[VM_REG_P1];

  if (query == 0u && MVM_RuntimeMemRangeOk(ctx, out, 8u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 8u);
    vm_write_u16_le(ctx->mem + out + 2u, 8u);
    vm_write_u16_le(ctx->mem + out + 4u, profile->screen_width);
    vm_write_u16_le(ctx->mem + out + 6u, profile->screen_height);
    result = 1u;
  }
  else if (query == 2u && MVM_RuntimeMemRangeOk(ctx, out, 4u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 4u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->color_mode);
    result = 1u;
  }
  else if (query == 3u && MVM_RuntimeMemRangeOk(ctx, out, 4u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 4u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->sound_flags);
    result = 1u;
  }
  else if (query == 4u && MVM_RuntimeMemRangeOk(ctx, out, 12u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 12u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->system_flags);
    vm_write_u32_le(ctx->mem + out + 4u, profile->device_id);
    vm_write_u32_le(ctx->mem + out + 8u, 0u);
    result = 1u;
  }

  return result;
} /* End of MVM_lPlatformGetCaps */

/**********************************************************************************************************************
 *  END OF FILE MVM_Lcfg.c
 *********************************************************************************************************************/
