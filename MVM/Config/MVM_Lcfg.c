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
typedef struct MVM_tstDefaultRuntimePool
{
  uint8_t au8Memory[MVM_CFG_RUNTIME_POOL_SIZE]; /**< Static VM runtime arena storage. */
} MVM_tstDefaultRuntimePool;

/**********************************************************************************************************************
 *  LOCAL FUNCTION PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Writes one diagnostic message through the default platform logger.
 */
static int MVM_LiDefaultLog(void *user, const char *message);

/**
 * @brief Handles platform-owned imports that complete with a zero result.
 */
static uint32_t MVM_Lu32PlatformReturnZero(MophunVM *vm, void *user);

/**
 * @brief Handles the platform-owned VM termination import.
 */
static uint32_t MVM_Lu32PlatformTerminate(MophunVM *vm, void *user);

/**
 * @brief Handles the platform-owned tick-count import.
 */
static uint32_t MVM_Lu32PlatformGetTickCount(MophunVM *vm, void *user);

/**
 * @brief Handles the platform-owned random-seed import.
 */
static uint32_t MVM_Lu32PlatformSetRandom(MophunVM *vm, void *user);

/**
 * @brief Handles the platform-owned random-value import.
 */
static uint32_t MVM_Lu32PlatformGetRandom(MophunVM *vm, void *user);

/**
 * @brief Handles the platform-owned capability-query import.
 */
static uint32_t MVM_Lu32PlatformGetCaps(MophunVM *vm, void *user);

/**********************************************************************************************************************
 *  LOCAL DATA
 *********************************************************************************************************************/

static MVM_tstDefaultRuntimePool MVM_LstDefaultRuntimePool;

static const MophunDeviceProfile MVM_LkastDeviceProfiles[] =
{
  {
    /* Human-readable profile identifier used by the host integration. */
    .name = "SE_T310",

    /* Visible framebuffer width reported to vGetCaps. */
    .screen_width = MVM_CFG_DEVICE_SCREEN_WIDTH,

    /* Visible framebuffer height reported to vGetCaps. */
    .screen_height = MVM_CFG_DEVICE_SCREEN_HEIGHT,

    /* Encoded display capability flags reported to the guest. */
    .color_mode = MVM_CFG_DEVICE_COLOR_MODE,

    /* Encoded audio capability flags reported to the guest. */
    .sound_flags = MVM_CFG_DEVICE_SOUND_FLAGS,

    /* Encoded system capability flags reported to the guest. */
    .system_flags = MVM_CFG_DEVICE_SYSTEM_FLAGS,

    /* Device identifier reported through the capability query API. */
    .device_id = MVM_CFG_DEVICE_ID
  }
};

static const MophunSyscall MVM_LkatPlatformSyscalls[] =
{
  /* Platform query and control imports implemented by this integration layer. */
  { .name = "vGetCaps", .fn = MVM_Lu32PlatformGetCaps, .user = NULL },
  { .name = "vGetTickCount", .fn = MVM_Lu32PlatformGetTickCount, .user = NULL },
  { .name = "vSetRandom", .fn = MVM_Lu32PlatformSetRandom, .user = NULL },
  { .name = "vGetRandom", .fn = MVM_Lu32PlatformGetRandom, .user = NULL },
  { .name = "vTerminateVMGP", .fn = MVM_Lu32PlatformTerminate, .user = NULL },

  /* Default platform stubs for graphics, audio, UI, and control imports. */
  { .name = "DbgPrintf", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vPrint", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vClearScreen", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vDrawLine", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vDrawObject", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vFillRect", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vFlipScreen", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vGetButtonData", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vGetPaletteEntry", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vMapDispose", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vMapGetAttribute", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vMapInit", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vMapSetTile", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vMapSetXY", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vMsgBox", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vPlayResource", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetActiveFont", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetBackColor", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetClipWindow", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetForeColor", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetPalette", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetPaletteEntry", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSetTransferMode", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSpriteBoxCollision", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSpriteCollision", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSpriteDispose", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSpriteInit", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSpriteSet", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vStreamWrite", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vSysCtl", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vTestKey", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vUpdateMap", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vUpdateSprite", .fn = MVM_Lu32PlatformReturnZero, .user = NULL },
  { .name = "vitoa", .fn = MVM_Lu32PlatformReturnZero, .user = NULL }
};

/**********************************************************************************************************************
 *  GLOBAL DATA
 *********************************************************************************************************************/

const MVM_tstConfig MVM_kstConfig =
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
    .log = MVM_LiDefaultLog
#else
    .log = NULL
#endif
  },

  /* Full device-profile catalog exposed by this integration. */
  .device_profiles = MVM_LkastDeviceProfiles,

  /* Number of entries available in the device-profile catalog. */
  .device_profile_count = (uint32_t)(sizeof(MVM_LkastDeviceProfiles) / sizeof(MVM_LkastDeviceProfiles[0])),

  /* Device profile returned through platform-owned vGetCaps queries. */
  .device_profile = &MVM_LkastDeviceProfiles[0],

  /* Syscall table for platform-owned VM imports. */
  .syscalls = MVM_LkatPlatformSyscalls,

  /* Number of entries visible in the platform syscall table. */
  .syscall_count = (uint32_t)(sizeof(MVM_LkatPlatformSyscalls) / sizeof(MVM_LkatPlatformSyscalls[0])),

  /* Static arena shared by guest RAM and VM-owned loader metadata. */
  .runtime_pool = MVM_LstDefaultRuntimePool.au8Memory,

  /* Total number of bytes available in the static runtime arena. */
  .runtime_pool_size = sizeof(MVM_LstDefaultRuntimePool.au8Memory),

  /* Default no-progress step budget. Zero keeps the watchdog disabled. */
  .watchdog_limit = MVM_CFG_DEFAULT_WATCHDOG_LIMIT
};

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_LiDefaultLog
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Writes one diagnostic message through the default platform logger.
 *********************************************************************************************************************/
static int MVM_LiDefaultLog(void *user, const char *message)
{
  (void)user;

  return fputs(message, stdout);
} /* End of MVM_LiDefaultLog */

/**********************************************************************************************************************
 *  Name: MVM_Lu32PlatformReturnZero
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles platform-owned imports that complete with a zero result.
 *********************************************************************************************************************/
static uint32_t MVM_Lu32PlatformReturnZero(MophunVM *vm, void *user)
{
  (void)vm;
  (void)user;

  return 0u;
} /* End of MVM_Lu32PlatformReturnZero */

/**********************************************************************************************************************
 *  Name: MVM_Lu32PlatformTerminate
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned VM termination import.
 *********************************************************************************************************************/
static uint32_t MVM_Lu32PlatformTerminate(MophunVM *vm, void *user)
{
  (void)user;

  MVM_vidRequestExit(vm);

  return 0u;
} /* End of MVM_Lu32PlatformTerminate */

/**********************************************************************************************************************
 *  Name: MVM_Lu32PlatformGetTickCount
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned tick-count import.
 *********************************************************************************************************************/
static uint32_t MVM_Lu32PlatformGetTickCount(MophunVM *vm, void *user)
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
} /* End of MVM_Lu32PlatformGetTickCount */

/**********************************************************************************************************************
 *  Name: MVM_Lu32PlatformSetRandom
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned random-seed import.
 *********************************************************************************************************************/
static uint32_t MVM_Lu32PlatformSetRandom(MophunVM *vm, void *user)
{
  VMGPContext *ctx = (VMGPContext *)vm;

  (void)user;

  if (!ctx)
  {
    return 0u;
  }

  ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;

  return 0u;
} /* End of MVM_Lu32PlatformSetRandom */

/**********************************************************************************************************************
 *  Name: MVM_Lu32PlatformGetRandom
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned random-value import.
 *********************************************************************************************************************/
static uint32_t MVM_Lu32PlatformGetRandom(MophunVM *vm, void *user)
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
} /* End of MVM_Lu32PlatformGetRandom */

/**********************************************************************************************************************
 *  Name: MVM_Lu32PlatformGetCaps
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned capability-query import.
 *********************************************************************************************************************/
static uint32_t MVM_Lu32PlatformGetCaps(MophunVM *vm, void *user)
{
  VMGPContext *ctx = (VMGPContext *)vm;
  const MophunDeviceProfile *profile = NULL;
  uint32_t query = 0;
  uint32_t out = 0;
  uint32_t u32Result = 0;

  (void)user;

  if (!ctx || !ctx->device_profile)
  {
    return 0u;
  }

  profile = ctx->device_profile;
  query = ctx->regs[VM_REG_P0];
  out = ctx->regs[VM_REG_P1];

  if (query == 0u && MVM_LbRuntimeMemRangeOk(ctx, out, 8u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 8u);
    vm_write_u16_le(ctx->mem + out + 2u, 8u);
    vm_write_u16_le(ctx->mem + out + 4u, profile->screen_width);
    vm_write_u16_le(ctx->mem + out + 6u, profile->screen_height);
    u32Result = 1u;
  }
  else if (query == 2u && MVM_LbRuntimeMemRangeOk(ctx, out, 4u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 4u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->color_mode);
    u32Result = 1u;
  }
  else if (query == 3u && MVM_LbRuntimeMemRangeOk(ctx, out, 4u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 4u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->sound_flags);
    u32Result = 1u;
  }
  else if (query == 4u && MVM_LbRuntimeMemRangeOk(ctx, out, 12u))
  {
    vm_write_u16_le(ctx->mem + out + 0u, 12u);
    vm_write_u16_le(ctx->mem + out + 2u, profile->system_flags);
    vm_write_u32_le(ctx->mem + out + 4u, profile->device_id);
    vm_write_u32_le(ctx->mem + out + 8u, 0u);
    u32Result = 1u;
  }

  return u32Result;
} /* End of MVM_Lu32PlatformGetCaps */

/**********************************************************************************************************************
 *  END OF FILE MVM_Lcfg.c
 *********************************************************************************************************************/
