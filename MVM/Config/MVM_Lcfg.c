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
#include <stdarg.h>
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
 * @brief Logs one platform-wrapper trace message through the VM logger.
 */
#if (MVM_MAX_LOG_LEVEL >= 2U)
static void MVM_lLogPlatformCall(MpnVM_t *vm, MVM_LogLevel_t level, const char *event, const char *fmt, ...);
#endif

/**
 * @brief Handles platform-owned imports that complete with a zero result.
 */
static uint32_t MVM_lPlatformRetZero(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned frame-present import.
 */
static uint32_t MVM_lPlatformFlipScreen(MpnVM_t *vm, void *user);

/**
 * @brief Handles the platform-owned sound-request import.
 */
static uint32_t MVM_lPlatformPlayResource(MpnVM_t *vm, void *user);

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

/**
 * @brief Reads one byte range from one file-backed image source.
 */
static int MVM_lReadFileImage(void *user, size_t offset, void *dst, size_t size);

/**********************************************************************************************************************
 *  LOCAL MACROS
 *********************************************************************************************************************/

#if (MVM_MAX_LOG_LEVEL >= 0U)
#define MVM_CFG_LOG_E(vm, event, fmt, ...) \
  MVM_lLogPlatformCall((vm), MVM_LOG_LEVEL_ERROR, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_CFG_LOG_E(vm, event, fmt, ...)                        ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 1U)
#define MVM_CFG_LOG_W(vm, event, fmt, ...) \
  MVM_lLogPlatformCall((vm), MVM_LOG_LEVEL_WARNING, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_CFG_LOG_W(vm, event, fmt, ...)                        ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 2U)
#define MVM_CFG_LOG_I(vm, event, fmt, ...) \
  MVM_lLogPlatformCall((vm), MVM_LOG_LEVEL_INFO, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_CFG_LOG_I(vm, event, fmt, ...)                        ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 3U)
#define MVM_CFG_LOG_D(vm, event, fmt, ...) \
  MVM_lLogPlatformCall((vm), MVM_LOG_LEVEL_DEBUG, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_CFG_LOG_D(vm, event, fmt, ...)                        ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 4U)
#define MVM_CFG_LOG_T(vm, event, fmt, ...) \
  MVM_lLogPlatformCall((vm), MVM_LOG_LEVEL_TRACE, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_CFG_LOG_T(vm, event, fmt, ...)                        ((void)0)
#endif

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
  { .name = "vGetCaps", .fn = MVM_lPlatformGetCaps, .user = "vGetCaps" },
  { .name = "vGetTickCount", .fn = MVM_lPlatformGetTickCount, .user = "vGetTickCount" },
  { .name = "vSetRandom", .fn = MVM_lPlatformSetRandom, .user = "vSetRandom" },
  { .name = "vGetRandom", .fn = MVM_lPlatformGetRandom, .user = "vGetRandom" },
  { .name = "vTerminateVMGP", .fn = MVM_lPlatformTerminate, .user = "vTerminateVMGP" },

  /* Default platform stubs for graphics, audio, UI, and control imports. */
  { .name = "DbgPrintf", .fn = MVM_lPlatformRetZero, .user = "DbgPrintf" },
  { .name = "vPrint", .fn = MVM_lPlatformRetZero, .user = "vPrint" },
  { .name = "vClearScreen", .fn = MVM_lPlatformRetZero, .user = "vClearScreen" },
  { .name = "vDrawLine", .fn = MVM_lPlatformRetZero, .user = "vDrawLine" },
  { .name = "vDrawObject", .fn = MVM_lPlatformRetZero, .user = "vDrawObject" },
  { .name = "vFillRect", .fn = MVM_lPlatformRetZero, .user = "vFillRect" },
  { .name = "vFlipScreen", .fn = MVM_lPlatformFlipScreen, .user = "vFlipScreen" },
  { .name = "vGetButtonData", .fn = MVM_lPlatformRetZero, .user = "vGetButtonData" },
  { .name = "vGetPaletteEntry", .fn = MVM_lPlatformRetZero, .user = "vGetPaletteEntry" },
  { .name = "vMapDispose", .fn = MVM_lPlatformRetZero, .user = "vMapDispose" },
  { .name = "vMapGetAttribute", .fn = MVM_lPlatformRetZero, .user = "vMapGetAttribute" },
  { .name = "vMapInit", .fn = MVM_lPlatformRetZero, .user = "vMapInit" },
  { .name = "vMapSetTile", .fn = MVM_lPlatformRetZero, .user = "vMapSetTile" },
  { .name = "vMapSetXY", .fn = MVM_lPlatformRetZero, .user = "vMapSetXY" },
  { .name = "vMsgBox", .fn = MVM_lPlatformRetZero, .user = "vMsgBox" },
  { .name = "vPlayResource", .fn = MVM_lPlatformPlayResource, .user = "vPlayResource" },
  { .name = "vSetActiveFont", .fn = MVM_lPlatformRetZero, .user = "vSetActiveFont" },
  { .name = "vSetBackColor", .fn = MVM_lPlatformRetZero, .user = "vSetBackColor" },
  { .name = "vSetClipWindow", .fn = MVM_lPlatformRetZero, .user = "vSetClipWindow" },
  { .name = "vSetForeColor", .fn = MVM_lPlatformRetZero, .user = "vSetForeColor" },
  { .name = "vSetPalette", .fn = MVM_lPlatformRetZero, .user = "vSetPalette" },
  { .name = "vSetPaletteEntry", .fn = MVM_lPlatformRetZero, .user = "vSetPaletteEntry" },
  { .name = "vSetTransferMode", .fn = MVM_lPlatformRetZero, .user = "vSetTransferMode" },
  { .name = "vSpriteBoxCollision", .fn = MVM_lPlatformRetZero, .user = "vSpriteBoxCollision" },
  { .name = "vSpriteCollision", .fn = MVM_lPlatformRetZero, .user = "vSpriteCollision" },
  { .name = "vSpriteDispose", .fn = MVM_lPlatformRetZero, .user = "vSpriteDispose" },
  { .name = "vSpriteInit", .fn = MVM_lPlatformRetZero, .user = "vSpriteInit" },
  { .name = "vSpriteSet", .fn = MVM_lPlatformRetZero, .user = "vSpriteSet" },
  { .name = "vStreamWrite", .fn = MVM_lPlatformRetZero, .user = "vStreamWrite" },
  { .name = "vSysCtl", .fn = MVM_lPlatformRetZero, .user = "vSysCtl" },
  { .name = "vTestKey", .fn = MVM_lPlatformRetZero, .user = "vTestKey" },
  { .name = "vUpdateMap", .fn = MVM_lPlatformRetZero, .user = "vUpdateMap" },
  { .name = "vUpdateSprite", .fn = MVM_lPlatformRetZero, .user = "vUpdateSprite" },
  { .name = "vitoa", .fn = MVM_lPlatformRetZero, .user = "vitoa" }
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
 *  Name: MVM_lLogPlatformCall
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Logs one platform-wrapper trace message through the VM logger.
 *********************************************************************************************************************/
#if (MVM_MAX_LOG_LEVEL >= 2U)
static void MVM_lLogPlatformCall(MpnVM_t *vm, MVM_LogLevel_t level, const char *event, const char *fmt, ...)
{
  VMGPContext *ctx = (VMGPContext *)vm;
  char buffer[MVM_LOG_BUFFER_SIZE];
  va_list ap;

  if (!ctx)
  {
    return;
  }

  va_start(ap, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  MVM_LOG_RAW(ctx, level, event, "%s", buffer);
} /* End of MVM_lLogPlatformCall */
#endif

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
  const char *import_name = (const char *)user;
  (void)import_name;

  MVM_CFG_LOG_D(vm,
                "platform-import",
                "%s(p0=%08X p1=%08X p2=%08X p3=%08X) -> 0\n",
                import_name ? import_name : "platform-import",
                vm ? vm->regs[VM_REG_P0] : 0u,
                vm ? vm->regs[VM_REG_P1] : 0u,
                vm ? vm->regs[VM_REG_P2] : 0u,
                vm ? vm->regs[VM_REG_P3] : 0u);

  return 0u;
} /* End of MVM_lPlatformRetZero */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformFlipScreen
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned frame-present import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformFlipScreen(MpnVM_t *vm, void *user)
{
  const char *import_name = (const char *)user;
  (void)import_name;

  MVM_CFG_LOG_D(vm,
                "frame-ready",
                "%s(p0=%08X p1=%08X p2=%08X p3=%08X)\n",
                import_name ? import_name : "vFlipScreen",
                vm ? vm->regs[VM_REG_P0] : 0u,
                vm ? vm->regs[VM_REG_P1] : 0u,
                vm ? vm->regs[VM_REG_P2] : 0u,
                vm ? vm->regs[VM_REG_P3] : 0u);
  MVM_EmitEvent((VMGPContext *)vm, MVM_EVENT_FRAME_READY, vm ? vm->regs[VM_REG_P0] : 0u, vm ? vm->pc : 0u);

  return 0u;
} /* End of MVM_lPlatformFlipScreen */

/**********************************************************************************************************************
 *  Name: MVM_lPlatformPlayResource
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles the platform-owned sound-request import.
 *********************************************************************************************************************/
static uint32_t MVM_lPlatformPlayResource(MpnVM_t *vm, void *user)
{
  const char *import_name = (const char *)user;
  (void)import_name;

  MVM_CFG_LOG_I(vm,
                "sound-request",
                "%s(resource=%08X p1=%08X p2=%08X p3=%08X)\n",
                import_name ? import_name : "vPlayResource",
                vm ? vm->regs[VM_REG_P0] : 0u,
                vm ? vm->regs[VM_REG_P1] : 0u,
                vm ? vm->regs[VM_REG_P2] : 0u,
                vm ? vm->regs[VM_REG_P3] : 0u);
  MVM_EmitEvent((VMGPContext *)vm,
                MVM_EVENT_SOUND_REQUESTED,
                vm ? vm->regs[VM_REG_P0] : 0u,
                vm ? vm->regs[VM_REG_P1] : 0u);

  return 0u;
} /* End of MVM_lPlatformPlayResource */

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
  const char *import_name = (const char *)user;
  (void)import_name;

  MVM_CFG_LOG_I(vm,
                "terminate",
                "%s()\n",
                import_name ? import_name : "vTerminateVMGP");

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
  const char *import_name = (const char *)user;
  (void)import_name;

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

  MVM_CFG_LOG_D(vm,
                "tick",
                "%s() -> %08X\n",
                import_name ? import_name : "vGetTickCount",
                ctx->tick_count);

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
  const char *import_name = (const char *)user;
  (void)import_name;

  if (!ctx)
  {
    return 0u;
  }

  ctx->random_state = ctx->regs[VM_REG_P0] ? ctx->regs[VM_REG_P0] : 1u;
  MVM_CFG_LOG_D(vm,
                "random-seed",
                "%s(seed=%08X)\n",
                import_name ? import_name : "vSetRandom",
                ctx->random_state);

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
  const char *import_name = (const char *)user;
  uint32_t value = 0u;
  (void)import_name;

  if (!ctx)
  {
    return 0u;
  }

  if (ctx->platform.get_random)
  {
    value = ctx->platform.get_random(ctx->platform.user);
  }
  else
  {
    ctx->random_state = ctx->random_state * 1103515245u + 12345u;
    value = (ctx->random_state >> 16) & 0xFFFFu;
  }

  MVM_CFG_LOG_D(vm,
                "random-value",
                "%s() -> %08X\n",
                import_name ? import_name : "vGetRandom",
                value);

  return value;
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
  const char *import_name = (const char *)user;
  (void)import_name;
  uint32_t query = 0;
  uint32_t out = 0;
  uint32_t result = 0;

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

  MVM_CFG_LOG_D(vm,
                "caps",
                "%s(query=%u out=%08X profile=%s) -> %u\n",
                import_name ? import_name : "vGetCaps",
                query,
                out,
                profile->name ? profile->name : "<unnamed>",
                result);

  return result;
} /* End of MVM_lPlatformGetCaps */

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
