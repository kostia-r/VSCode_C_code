/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_VmgpLoader.c
 *           Module:  MVM_Loader
 *           Target:  Portable C
 *      Description:  VMGP image parsing, pool loading, resource indexing, and initial memory construction.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(VMGPHeader) == VMGP_HEADER_SIZE, "VMGPHeader layout must stay aligned with decoded header size");

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Loads VMGP resource table metadata.
 */
static bool MVM_lLoadVmgpResources(VMGPContext *ctx);

/**
 * @brief Builds the initial VM memory image.
 */
static bool MVM_lBuildVmgpMemory(VMGPContext *ctx);

/**
 * @brief Applies one minimal relocation/fixup pass to guest data memory.
 */
static bool MVM_lApplyVmgpDataRelocations(VMGPContext *ctx);

/**
 * @brief Resolves one pool value by 1-based pool index.
 */
static uint32_t MVM_lResolvePoolIndexValue(const VMGPContext *ctx, uint32_t pool_index_1based, uint32_t depth);

/**
 * @brief Reads one 32-bit little-endian word from the active image source.
 */
static bool MVM_lReadImageWord(const VMGPContext *ctx, size_t offset, uint32_t *out);

/**
 * @brief Returns a string-table pointer from a file offset.
 */
static const char *vm_file_str(const VMGPContext *ctx, uint32_t off);

/**
 * @brief Aligns one size value for runtime-pool planning.
 */
static size_t MVM_lAlignPoolSize(size_t value);

/**
 * @brief Reads one byte range while planning memory without constructing a complete VM context.
 */
static bool MVM_lReadQueryImage(const MpnImageSource_t *image,
                                const MVM_Config_t *config,
                                size_t offset,
                                void *dst,
                                size_t size);

/**
 * @brief Parses the VMGP header fields required by memory planning.
 */
static bool MVM_lParseQueryHeader(const MpnImageSource_t *image,
                                  const MVM_Config_t *config,
                                  VMGPHeader *header,
                                  size_t *res_file_offset,
                                  uint32_t *res_offset);

/**
 * @brief Counts resource entries while planning memory without a complete VM context.
 */
static uint32_t MVM_lCountQueryResources(const MpnImageSource_t *image,
                                         const MVM_Config_t *config,
                                         size_t res_file_offset,
                                         uint32_t res_size);

/**
 * @brief Reads one byte range from a memory-backed VM image.
 */
static int MVM_lReadMemoryImage(void *user, size_t offset, void *dst, size_t size);

/**
 * @brief Queries memory requirements for one image source with one explicit backend config.
 */
static MVM_RetCode_t MVM_lQueryMemReqsWithConfig(const MpnImageSource_t *image,
                                                 const MVM_Config_t *config,
                                                 MVM_MemReqs_t *requirements);

/**
 * @brief Queries memory requirements for one source-backed VMGP image.
 */
MVM_RetCode_t MVM_QueryMemReqsFromSourceWithConfig(const MpnImageSource_t *image,
                                                   const MVM_Config_t *config,
                                                   MVM_MemReqs_t *requirements);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_GetVmgpPoolTypeName
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VMGP pool data.
 *********************************************************************************************************************/
const char *MVM_GetVmgpPoolTypeName(uint8_t type)
{
  const char *name = "unknown";

  switch (type)
  {
    case 0x00:
    {
      name = "null";
      break;
    } /* End of case 0x00 */

    case 0x02:
    {
      name = "import";
      break;
    } /* End of case 0x02 */

    case 0x11:
    {
      name = "code";
      break;
    } /* End of case 0x11 */

    case 0x13:
    {
      name = "export";
      break;
    } /* End of case 0x13 */

    case 0x18:
    {
      name = "bytes";
      break;
    } /* End of case 0x18 */

    case 0x21:
    {
      name = "u32";
      break;
    } /* End of case 0x21 */

    case 0x23:
    {
      name = "const?";
      break;
    } /* End of case 0x23 */

    case 0x24:
    {
      name = "str?";
      break;
    } /* End of case 0x24 */

    case 0x25:
    {
      name = "res?";
      break;
    } /* End of case 0x25 */

    case 0x26:
    {
      name = "ptr?";
      break;
    } /* End of case 0x26 */

    case 0x41:
    {
      name = "addr?";
      break;
    } /* End of case 0x41 */

    case 0x67:
    {
      name = "sys?";
      break;
    } /* End of case 0x67 */

    default:
    {
      break;
    } /* End of default */
  } /* End of switch */

  return name;
} /* End of MVM_GetVmgpPoolTypeName */

/**********************************************************************************************************************
 *  Name: MVM_GetVmgpPoolSizeBytes
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VMGP pool data.
 *********************************************************************************************************************/
size_t MVM_GetVmgpPoolSizeBytes(const VMGPHeader *header)
{
  size_t sizeBytes = 0;

  sizeBytes = header ? (size_t)header->pool_slots * VMGP_POOL_SLOT_SIZE : 0u;

  return sizeBytes;
} /* End of MVM_GetVmgpPoolSizeBytes */

/**********************************************************************************************************************
 *  Name: MVM_GetVmgpPoolEntry
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VMGP pool data.
 *********************************************************************************************************************/
const VMGPPoolEntry *MVM_GetVmgpPoolEntry(const VMGPContext *ctx, uint32_t pool_index_1based)
{
  const VMGPPoolEntry *entry = NULL;

  if (!ctx || !ctx->pool || pool_index_1based == 0 || pool_index_1based > ctx->header.pool_slots)
  {
    return NULL;
  }

  entry = &ctx->pool[pool_index_1based - 1u];

  return entry;
} /* End of MVM_GetVmgpPoolEntry */

/**********************************************************************************************************************
 *  Name: MVM_ResolveVmgpPoolValue
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VMGP pool data.
 *********************************************************************************************************************/
uint32_t MVM_ResolveVmgpPoolValue(const VMGPContext *ctx, const VMGPPoolEntry *entry)
{
  uint32_t value = 0;
  uint8_t poolType;
  uint8_t itemTarget;

  if (!ctx || !entry)
  {
    return 0;
  }

  poolType = (uint8_t)(entry->type & 0x0Fu);
  itemTarget = (uint8_t)(entry->type >> 4);

  switch (poolType)
  {
    case 0x01: /* local symbol */

    case 0x03: /* global symbol */
    {
      switch (itemTarget)
      {
        case 0x01u:
        {
          value = ctx->code_offset + entry->value;
          break;
        }

        case 0x02u:
        {
          value = ctx->data_offset + entry->value;
          break;
        }

        case 0x04u:
        {
          value = ctx->bss_offset + entry->value;
          break;
        }

        default:
        {
          value = entry->value;
          break;
        }
      }
      break;
    } /* End of case 0x03 */

    case 0x07: /* const32 */
    {
      value = entry->value;
      break;
    } /* End of case 0x07 */

    case 0x08: /* symbol-add */
    {
      value = MVM_lResolvePoolIndexValue(ctx, entry->aux24, 0u) + entry->value;
      break;
    } /* End of case 0x08 */

    case 0x04: /* section-relative relocation */

    case 0x05: /* swap16 relocation */

    case 0x06: /* swap32 relocation */

    case 0x02: /* import symbol */

    default:
    {
      value = entry->value;
      break;
    } /* End of default */
  } /* End of switch */

  return value;
} /* End of MVM_ResolveVmgpPoolValue */

/**********************************************************************************************************************
 *  Name: MVM_GetVmgpImportName
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
const char *MVM_GetVmgpImportName(const VMGPContext *ctx, uint32_t pool_index_1based)
{
  static char bad[32];
  const VMGPPoolEntry *entry = NULL;
  const char *name = NULL;
  const char *importName = NULL;

  entry = MVM_GetVmgpPoolEntry(ctx, pool_index_1based);

  if (!entry)
  {
    snprintf(bad, sizeof(bad), "<bad:%u>", pool_index_1based);

    return bad;
  }

  if (entry->type != 0x02)
  {
    snprintf(bad, sizeof(bad), "<type:%02X>", entry->type);

    return bad;
  }

  name = vm_file_str(ctx, entry->aux24);
  importName = name ? name : "<str-oob>";

  return importName;
} /* End of MVM_GetVmgpImportName */

/**********************************************************************************************************************
 *  Name: MVM_lResolvePoolIndexValue
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Resolves one pool value recursively by 1-based index.
 *********************************************************************************************************************/
static uint32_t MVM_lResolvePoolIndexValue(const VMGPContext *ctx, uint32_t pool_index_1based, uint32_t depth)
{
  const VMGPPoolEntry *entry;
  uint8_t poolType;

  if (!ctx || depth > 32u)
  {
    return 0u;
  }

  entry = MVM_GetVmgpPoolEntry(ctx, pool_index_1based);
  if (!entry)
  {
    return 0u;
  }

  poolType = (uint8_t)(entry->type & 0x0Fu);
  if (poolType == 0x08u)
  {
    return MVM_lResolvePoolIndexValue(ctx, entry->aux24, depth + 1u) + entry->value;
  }

  return MVM_ResolveVmgpPoolValue(ctx, entry);
} /* End of MVM_lResolvePoolIndexValue */

/**********************************************************************************************************************
 *  Name: MVM_GetVmgpResource
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
const VMGPResource *MVM_GetVmgpResource(const VMGPContext *ctx, uint32_t resource_id)
{
  uint32_t i;
  const VMGPResource *resource = NULL;

  if (!ctx || !ctx->resources || resource_id == 0)
  {
    return NULL;
  }

  for (i = 0; i < ctx->resource_count; ++i)
  {
    if (ctx->resources[i].id == resource_id)
    {
      resource = &ctx->resources[i];
      break;
    }
  } /* End of loop */

  return resource;
} /* End of MVM_GetVmgpResource */

/**********************************************************************************************************************
 *  Name: MVM_ParseVmgpHeaderRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Parses VMGP image data.
 *********************************************************************************************************************/
bool MVM_ParseVmgpHeaderRaw(VMGPContext *ctx)
{
  uint8_t headerBytes[VMGP_HEADER_SIZE];

  if (!ctx || ctx->size < VMGP_HEADER_SIZE)
  {
    return false;
  }

  if (!MVM_ReadImageRange(ctx, 0u, headerBytes, sizeof(headerBytes)))
  {
    return false;
  }

  memcpy(ctx->header.magic, headerBytes + 0x00, 4);
  ctx->header.data_heap_words = vm_read_u16_le(headerBytes + 0x04);
  ctx->header.unknown2 = vm_read_u16_le(headerBytes + 0x06);
  ctx->header.stack_words = vm_read_u16_le(headerBytes + 0x08);
  ctx->header.unknown3 = headerBytes[0x0A];
  ctx->header.unknown4 = headerBytes[0x0B];
  ctx->header.code_size = vm_read_u32_le(headerBytes + 0x0C);
  ctx->header.data_size = vm_read_u32_le(headerBytes + 0x10);
  ctx->header.bss_size = vm_read_u32_le(headerBytes + 0x14);
  ctx->header.res_size = vm_read_u32_le(headerBytes + 0x18);
  ctx->header.unknown5 = vm_read_u32_le(headerBytes + 0x1C);
  ctx->header.pool_slots = vm_read_u32_le(headerBytes + 0x20);
  ctx->header.string_size = vm_read_u32_le(headerBytes + 0x24);

  ctx->header_valid = (memcmp(ctx->header.magic, VMGP_MAGIC, 4) == 0);

  if (!ctx->header_valid)
  {
    return false;
  }

  ctx->code_file_offset = VMGP_HEADER_SIZE;
  ctx->data_file_offset = ctx->code_file_offset + ctx->header.code_size;
  ctx->code_offset = 0;
  ctx->data_offset = 0;
  ctx->bss_offset = ctx->data_offset + ctx->header.data_size;
  ctx->res_offset = ctx->bss_offset + ctx->header.bss_size;
  ctx->res_file_offset = ctx->data_file_offset + ctx->header.data_size;
  ctx->pool_offset = ctx->res_file_offset + ctx->header.res_size;
  ctx->strtab_offset = ctx->pool_offset + ctx->header.pool_slots * VMGP_POOL_SLOT_SIZE;

  if ((size_t)ctx->strtab_offset + ctx->header.string_size > ctx->size)
  {
    return false;
  }

  return true;
} /* End of MVM_ParseVmgpHeaderRaw */

/**********************************************************************************************************************
 *  Name: MVM_ParseVmgpHeader
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Public wrapper for VMGP header parsing.
 *********************************************************************************************************************/
bool MVM_ParseVmgpHeader(VMGPContext *ctx)
{
  return MVM_ParseVmgpHeaderRaw(ctx);
} /* End of MVM_ParseVmgpHeader */

/**********************************************************************************************************************
 *  Name: MVM_QueryMemReqsWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Queries static memory requirements using one parent-owned integration config.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_QueryMemReqsWithConfig(const uint8_t *image,
                                         size_t image_size,
                                         const MVM_Config_t *config,
                                         MVM_MemReqs_t *requirements)
{
  MpnImageSource_t source;
  MVM_Config_t selectedConfig;

  if (!image || image_size < VMGP_HEADER_SIZE || !config)
  {
    return MVM_INVALID_ARG;
  }

  selectedConfig = *config;

  memset(&source, 0, sizeof(source));
  source.user = (void *)image;
  source.image_size = image_size;

  selectedConfig.image_read = MVM_lReadMemoryImage;
  selectedConfig.image_write = NULL;
  selectedConfig.image_map = NULL;
  selectedConfig.image_unmap = NULL;

  return MVM_lQueryMemReqsWithConfig(&source, &selectedConfig, requirements);
} /* End of MVM_QueryMemReqsWithConfig */

/**********************************************************************************************************************
 *  Name: MVM_QueryMemReqs
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Queries static memory requirements using the built-in integration config.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_QueryMemReqs(const uint8_t *image,
                               size_t image_size,
                               MVM_MemReqs_t *requirements)
{
  return MVM_QueryMemReqsWithConfig(image, image_size, &MVM_Config, requirements);
} /* End of MVM_QueryMemReqs */

/**********************************************************************************************************************
 *  Name: MVM_QueryMemReqsFromSourceWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Queries source-backed memory requirements using one parent-owned integration config.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_QueryMemReqsFromSourceWithConfig(const MpnImageSource_t *image,
                                                   const MVM_Config_t *config,
                                                   MVM_MemReqs_t *requirements)
{
  return MVM_lQueryMemReqsWithConfig(image, config, requirements);
} /* End of MVM_QueryMemReqsFromSourceWithConfig */

/**********************************************************************************************************************
 *  Name: MVM_QueryMemReqsFromSource
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Queries source-backed memory requirements using the built-in integration config.
 *********************************************************************************************************************/
MVM_RetCode_t MVM_QueryMemReqsFromSource(const MpnImageSource_t *image, MVM_MemReqs_t *requirements)
{
  return MVM_QueryMemReqsFromSourceWithConfig(image, &MVM_Config, requirements);
} /* End of MVM_QueryMemReqsFromSource */

/**********************************************************************************************************************
 *  Name: MVM_LoadVmgpPoolRaw
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Loads VMGP image data.
 *********************************************************************************************************************/
bool MVM_LoadVmgpPoolRaw(VMGPContext *ctx)
{
  uint32_t i;
  uint8_t entryBytes[VMGP_POOL_SLOT_SIZE];

  if (!ctx || !ctx->header_valid)
  {
    return false;
  }

  ctx->pool = (VMGPPoolEntry *)MVM_AcquireInitBuffer(ctx,
                                                     (size_t)ctx->header.pool_slots * sizeof(VMGPPoolEntry));

  if (!ctx->pool)
  {
    MVM_SetErrorRaw(ctx, MVM_E_MEMORY);

    return false;
  }

  if (ctx->header.string_size > 0u)
  {
    ctx->strtab = (uint8_t *)MVM_AcquireInitBuffer(ctx, ctx->header.string_size);

    if (!ctx->strtab || !MVM_ReadImageRange(ctx, ctx->strtab_offset, ctx->strtab, ctx->header.string_size))
    {
      MVM_SetErrorRaw(ctx, MVM_E_MEMORY);

      return false;
    }
  }

  for (i = 0; i < ctx->header.pool_slots; ++i)
  {
    if (!MVM_ReadImageRange(ctx,
                            ctx->pool_offset + (size_t)i * VMGP_POOL_SLOT_SIZE,
                            entryBytes,
                            sizeof(entryBytes)))
    {
      return false;
    }

    ctx->pool[i].type = entryBytes[0];
    ctx->pool[i].aux24 = (uint32_t)entryBytes[1] |
                         ((uint32_t)entryBytes[2] << 8) |
                         ((uint32_t)entryBytes[3] << 16);
    ctx->pool[i].value = vm_read_u32_le(entryBytes + 4);
  } /* End of loop */

  if (!MVM_lLoadVmgpResources(ctx))
  {
    return false;
  }

  if (!MVM_lBuildVmgpMemory(ctx))
  {
    return false;
  }

  return true;
} /* End of MVM_LoadVmgpPoolRaw */

/**********************************************************************************************************************
 *  Name: MVM_LoadVmgpPool
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Public wrapper for VMGP pool loading.
 *********************************************************************************************************************/
bool MVM_LoadVmgpPool(VMGPContext *ctx)
{
  return MVM_LoadVmgpPoolRaw(ctx);
} /* End of MVM_LoadVmgpPool */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_lQueryMemReqsWithConfig
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Queries static memory requirements for one image source with one explicit backend config.
 *********************************************************************************************************************/
static MVM_RetCode_t MVM_lQueryMemReqsWithConfig(const MpnImageSource_t *image,
                                                 const MVM_Config_t *config,
                                                 MVM_MemReqs_t *requirements)
{
  VMGPHeader header;
  uint32_t resource_count = 0;
  uint32_t res_offset = 0;
  size_t res_file_offset = 0;
  size_t heapBytes = 0;
  size_t stackBytes = 0;
  size_t guestMemoryBytes = 0;
  size_t logicalMemoryBytes = 0;
  size_t highMemoryBytes = 0;
  size_t runtimePoolBytes = 0;
  size_t framebufferPixelCount = 0;

  if (!requirements)
  {
    return MVM_INVALID_ARG;
  }

  memset(requirements, 0, sizeof(*requirements));

  if (!image || !config || (!config->image_read && !config->drivers.image_read) ||
      image->image_size < VMGP_HEADER_SIZE)
  {
    return MVM_INVALID_ARG;
  }

  memset(&header, 0, sizeof(header));
  if (!MVM_lParseQueryHeader(image, config, &header, &res_file_offset, &res_offset))
  {
    return MVM_INIT_FAILED;
  }

  resource_count = MVM_lCountQueryResources(image, config, res_file_offset, header.res_size);
  heapBytes = VM_HEAP_EXTRA;
  stackBytes = (size_t)header.stack_words * sizeof(uint32_t);
  guestMemoryBytes = (size_t)vm_align4(res_offset);
  if (heapBytes > SIZE_MAX - guestMemoryBytes)
  {
    return MVM_MEMORY_ERROR;
  }
  guestMemoryBytes += heapBytes;
  logicalMemoryBytes = (size_t)vm_align4(res_offset);
  if (VM_LOGICAL_HEAP_EXTRA > SIZE_MAX - logicalMemoryBytes)
  {
    return MVM_MEMORY_ERROR;
  }
  logicalMemoryBytes += VM_LOGICAL_HEAP_EXTRA;
  if (stackBytes > SIZE_MAX - logicalMemoryBytes)
  {
    return MVM_MEMORY_ERROR;
  }
  logicalMemoryBytes += stackBytes;
  if (VM_STACK_GUARD_EXTRA > SIZE_MAX - logicalMemoryBytes || logicalMemoryBytes < VM_HIGH_MEMORY_BASE)
  {
    return MVM_MEMORY_ERROR;
  }
  logicalMemoryBytes += VM_STACK_GUARD_EXTRA;
  highMemoryBytes = logicalMemoryBytes - VM_HIGH_MEMORY_BASE;
  if (highMemoryBytes > SIZE_MAX - guestMemoryBytes)
  {
    return MVM_MEMORY_ERROR;
  }
  guestMemoryBytes += highMemoryBytes;

  requirements->guest_memory_bytes = guestMemoryBytes;
  requirements->pool_entries_bytes = (size_t)header.pool_slots * sizeof(VMGPPoolEntry);
  requirements->resource_entries_bytes = (size_t)resource_count * sizeof(VMGPResource);
  runtimePoolBytes = MVM_lAlignPoolSize(0u);
  runtimePoolBytes += requirements->pool_entries_bytes;
  runtimePoolBytes = MVM_lAlignPoolSize(runtimePoolBytes);
  runtimePoolBytes += requirements->resource_entries_bytes;
  runtimePoolBytes = MVM_lAlignPoolSize(runtimePoolBytes);
  runtimePoolBytes += header.string_size;
  runtimePoolBytes = MVM_lAlignPoolSize(runtimePoolBytes);
  runtimePoolBytes += requirements->guest_memory_bytes;
  if (config->drivers.display_flush && config->device_profile)
  {
    framebufferPixelCount = (size_t)config->device_profile->screen_width *
                            (size_t)config->device_profile->screen_height;
    if (framebufferPixelCount > (SIZE_MAX / sizeof(uint16_t)))
    {
      return MVM_MEMORY_ERROR;
    }

    runtimePoolBytes = MVM_lAlignPoolSize(runtimePoolBytes);
    if (runtimePoolBytes > SIZE_MAX - (framebufferPixelCount * sizeof(uint16_t)))
    {
      return MVM_MEMORY_ERROR;
    }

    runtimePoolBytes += framebufferPixelCount * sizeof(uint16_t);
  }
  requirements->runtime_pool_bytes = runtimePoolBytes;
  requirements->pool_entry_count = header.pool_slots;
  requirements->resource_count = resource_count;
  requirements->static_data_bytes = header.data_size;
  requirements->bss_bytes = header.bss_size;
  requirements->resource_bytes = 0u;
  requirements->heap_bytes = (uint32_t)heapBytes;
  requirements->stack_bytes = (uint32_t)stackBytes;

  return MVM_OK;
} /* End of MVM_lQueryMemReqsWithConfig */

/**********************************************************************************************************************
 *  Name: MVM_lReadQueryImage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: Yes
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Reads one image range through a parent config without constructing a complete VM context.
 *********************************************************************************************************************/
static bool MVM_lReadQueryImage(const MpnImageSource_t *image,
                                const MVM_Config_t *config,
                                size_t offset,
                                void *dst,
                                size_t size)
{
  MpnImageReadFn_t imageRead;

  if (!image || !config || !dst || offset > image->image_size || size > image->image_size - offset)
  {
    return false;
  }

  if (size == 0u)
  {
    return true;
  }

  imageRead = config->image_read ? config->image_read : config->drivers.image_read;

  return imageRead && imageRead(image->user, offset, dst, size) == 0;
} /* End of MVM_lReadQueryImage */

/**********************************************************************************************************************
 *  Name: MVM_lParseQueryHeader
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: Yes
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Parses VMGP layout and authored memory requirements into one small planning structure.
 *********************************************************************************************************************/
static bool MVM_lParseQueryHeader(const MpnImageSource_t *image,
                                  const MVM_Config_t *config,
                                  VMGPHeader *header,
                                  size_t *res_file_offset,
                                  uint32_t *res_offset)
{
  uint8_t headerBytes[VMGP_HEADER_SIZE];
  size_t sectionOffset;

  if (!header || !res_file_offset || !res_offset ||
      !MVM_lReadQueryImage(image, config, 0u, headerBytes, sizeof(headerBytes)))
  {
    return false;
  }

  memcpy(header->magic, headerBytes + 0x00, 4u);
  header->data_heap_words = vm_read_u16_le(headerBytes + 0x04);
  header->unknown2 = vm_read_u16_le(headerBytes + 0x06);
  header->stack_words = vm_read_u16_le(headerBytes + 0x08);
  header->unknown3 = headerBytes[0x0A];
  header->unknown4 = headerBytes[0x0B];
  header->code_size = vm_read_u32_le(headerBytes + 0x0C);
  header->data_size = vm_read_u32_le(headerBytes + 0x10);
  header->bss_size = vm_read_u32_le(headerBytes + 0x14);
  header->res_size = vm_read_u32_le(headerBytes + 0x18);
  header->unknown5 = vm_read_u32_le(headerBytes + 0x1C);
  header->pool_slots = vm_read_u32_le(headerBytes + 0x20);
  header->string_size = vm_read_u32_le(headerBytes + 0x24);

  if (memcmp(header->magic, VMGP_MAGIC, 4u) != 0 ||
      header->data_size > UINT32_MAX - header->bss_size)
  {
    return false;
  }

  *res_offset = header->data_size + header->bss_size;
  sectionOffset = VMGP_HEADER_SIZE;
  if (header->code_size > image->image_size - sectionOffset)
  {
    return false;
  }
  sectionOffset += header->code_size;
  if (header->data_size > image->image_size - sectionOffset)
  {
    return false;
  }
  sectionOffset += header->data_size;
  *res_file_offset = sectionOffset;
  if (header->res_size > image->image_size - sectionOffset)
  {
    return false;
  }
  sectionOffset += header->res_size;
  if (header->pool_slots > (image->image_size - sectionOffset) / VMGP_POOL_SLOT_SIZE)
  {
    return false;
  }
  sectionOffset += (size_t)header->pool_slots * VMGP_POOL_SLOT_SIZE;
  if (header->string_size > image->image_size - sectionOffset)
  {
    return false;
  }

  return true;
} /* End of MVM_lParseQueryHeader */

/**********************************************************************************************************************
 *  Name: MVM_lCountQueryResources
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: Yes
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Counts resource offsets through the small memory-query path.
 *********************************************************************************************************************/
static uint32_t MVM_lCountQueryResources(const MpnImageSource_t *image,
                                         const MVM_Config_t *config,
                                         size_t res_file_offset,
                                         uint32_t res_size)
{
  uint8_t bytes[4];
  uint32_t previous;
  uint32_t count;
  uint32_t index;
  uint32_t offset;

  previous = 0u;
  count = 0u;
  if (res_size < 8u)
  {
    return 0u;
  }

  for (index = 0u; index + sizeof(bytes) <= res_size; index += sizeof(bytes))
  {
    if (!MVM_lReadQueryImage(image, config, res_file_offset + index, bytes, sizeof(bytes)))
    {
      break;
    }

    offset = vm_read_u32_le(bytes);
    if (offset == 0u || offset >= res_size || offset < previous)
    {
      break;
    }

    previous = offset;
    ++count;
  } /* End of loop */

  return count;
} /* End of MVM_lCountQueryResources */

/**********************************************************************************************************************
 *  Name: MVM_lLoadVmgpResources
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Loads VMGP image data.
 *********************************************************************************************************************/
static bool MVM_lLoadVmgpResources(VMGPContext *ctx)
{
  uint32_t prev = 0;
  uint32_t count = 0;
  uint32_t i;
  uint32_t off = 0;
  uint32_t next = 0;

  if (!ctx || ctx->header.res_size < 8)
  {
    return true;
  }

  for (i = 0; i + 4 <= ctx->header.res_size; i += 4)
  {
    if (!MVM_lReadImageWord(ctx, ctx->res_file_offset + i, &off))
    {
      break;
    }

    if (off == 0)
    {
      break;
    }

    if (off >= ctx->header.res_size || off < prev)
    {
      break;
    }

    prev = off;
    count++;
  } /* End of loop */

  if (count == 0)
  {
    return true;
  }

  ctx->resources = (VMGPResource *)MVM_AcquireInitBuffer(ctx, (size_t)count * sizeof(VMGPResource));

  if (!ctx->resources)
  {
    MVM_SetErrorRaw(ctx, MVM_E_MEMORY);

    return false;
  }

  ctx->resource_count = count;

  for (i = 0; i < count; ++i)
  {
    if (!MVM_lReadImageWord(ctx, ctx->res_file_offset + (size_t)i * 4u, &off))
    {
      return false;
    }

    if (i + 1u < count)
    {
      if (!MVM_lReadImageWord(ctx, ctx->res_file_offset + (size_t)(i + 1u) * 4u, &next))
      {
        return false;
      }
    }
    else
    {
      next = ctx->header.res_size;
    }

    ctx->resources[i].id = i;
    ctx->resources[i].offset = off;
    ctx->resources[i].size = (next > off) ? (next - off) : 0u;
  } /* End of loop */

  return true;
} /* End of MVM_lLoadVmgpResources */

/**********************************************************************************************************************
 *  Name: MVM_lBuildVmgpMemory
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles VM memory diagnostics.
 *********************************************************************************************************************/
static bool MVM_lBuildVmgpMemory(VMGPContext *ctx)
{
  bool bResult = false;
  uint32_t heapBytes;
  uint32_t logicalHeapBytes;
  uint32_t stackBytes;
  uint32_t highMemoryBytes;

  ctx->vm_end = ctx->res_offset;
  ctx->heap_base = vm_align4(ctx->vm_end);
  ctx->heap_cur = ctx->heap_base;
  /* Keep query/init sizing identical while the monotonic heap is validated against long gameplay. */
  heapBytes = VM_HEAP_EXTRA;
  logicalHeapBytes = VM_LOGICAL_HEAP_EXTRA;
  stackBytes = (uint32_t)ctx->header.stack_words * sizeof(uint32_t);
  if (heapBytes > UINT32_MAX - ctx->heap_base)
  {
    return false;
  }
  ctx->heap_limit = ctx->heap_base + heapBytes;
  ctx->heap_soft_limit = ctx->heap_limit;
  if (ctx->header.data_heap_words != 0u &&
      ((uint32_t)ctx->header.data_heap_words * sizeof(uint32_t)) < heapBytes)
  {
    ctx->heap_soft_limit = ctx->heap_base +
                           ((uint32_t)ctx->header.data_heap_words * sizeof(uint32_t));
  }
  if (logicalHeapBytes > UINT32_MAX - ctx->heap_base ||
      stackBytes > UINT32_MAX - (ctx->heap_base + logicalHeapBytes))
  {
    return false;
  }
  ctx->stack_top = ctx->heap_base + logicalHeapBytes + stackBytes;
  if (VM_STACK_GUARD_EXTRA > UINT32_MAX - ctx->stack_top)
  {
    return false;
  }
  ctx->mem_size = ctx->stack_top + VM_STACK_GUARD_EXTRA;

  ctx->mem_low_size = ctx->heap_limit;
  if (ctx->mem_low_size > VM_HIGH_MEMORY_BASE || ctx->mem_size < VM_HIGH_MEMORY_BASE)
  {
    return false;
  }
  ctx->mem_high_base = VM_HIGH_MEMORY_BASE;
  highMemoryBytes = (uint32_t)(ctx->mem_size - ctx->mem_high_base);
  ctx->mem_high_size = highMemoryBytes;

  ctx->mem = (uint8_t *)MVM_AcquireInitBuffer(ctx, ctx->mem_low_size);
  ctx->mem_high = (uint8_t *)MVM_AcquireInitBuffer(ctx, ctx->mem_high_size);

  if (!ctx->mem || !ctx->mem_high)
  {
    MVM_SetErrorRaw(ctx, MVM_E_MEMORY);

    return false;
  }

  if (!MVM_ReadImageRange(ctx,
                          ctx->data_file_offset,
                          MVM_GUEST_PTR(ctx, ctx->data_offset, ctx->header.data_size),
                          ctx->header.data_size))
  {
    return false;
  }

  if (!MVM_lApplyVmgpDataRelocations(ctx))
  {
    return false;
  }

  ctx->pc = 0;
  ctx->regs[VM_REG_SP] = ctx->stack_top;
  ctx->regs[VM_REG_ZERO] = 0;
  bResult = true;

  return bResult;
} /* End of MVM_lBuildVmgpMemory */

/**********************************************************************************************************************
 *  Name: MVM_lApplyVmgpDataRelocations
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Applies one minimal relocation/fixup pass to guest data memory.
 *********************************************************************************************************************/
static bool MVM_lApplyVmgpDataRelocations(VMGPContext *ctx)
{
  uint32_t i;
  const VMGPPoolEntry *entry;
  uint8_t poolType;
  uint8_t itemTarget;
  uint32_t targetAddr;
  uint32_t patchedValue;

  if (!ctx || !ctx->mem || !ctx->pool)
  {
    return false;
  }

  for (i = 1u; i <= ctx->header.pool_slots; ++i)
  {
    entry = MVM_GetVmgpPoolEntry(ctx, i);
    if (!entry)
    {
      continue;
    }

    poolType = (uint8_t)(entry->type & 0x0Fu);
    itemTarget = (uint8_t)(entry->type >> 4);
    targetAddr = ctx->data_offset + entry->value;

    if (itemTarget != 0x02u || targetAddr >= ctx->header.data_size)
    {
      continue;
    }

    switch (poolType)
    {
      case 0x04u: /* section-relative relocation */
      {
        if (!MVM_RuntimeMemRangeOk(ctx, targetAddr, 4u))
        {
          return false;
        }

        patchedValue = vm_read_u32_le(MVM_GUEST_CONST_PTR(ctx, targetAddr, 4u));
        switch (entry->aux24)
        {
          case 0x01u:
          {
            patchedValue += ctx->code_offset;
            break;
          }

          case 0x02u:
          {
            patchedValue += ctx->data_offset;
            break;
          }

          case 0x04u:
          {
            patchedValue += ctx->bss_offset;
            break;
          }

          default:
          {
            break;
          }
        }

        vm_write_u32_le(MVM_GUEST_PTR(ctx, targetAddr, 4u), patchedValue);
        break;
      }

      case 0x05u: /* swap16 relocation */
      {
        /* Little-endian host: guest image bytes are already in the desired order. */
        break;
      }

      case 0x06u: /* swap32 relocation */
      {
        /* Little-endian host: guest image bytes are already in the desired order. */
        break;
      }

      default:
      {
        break;
      }
    }
  }

  return true;
} /* End of MVM_lApplyVmgpDataRelocations */

/**********************************************************************************************************************
 *  Name: MVM_lReadImageWord
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Reads one little-endian 32-bit word from the active image source.
 *********************************************************************************************************************/
static bool MVM_lReadImageWord(const VMGPContext *ctx, size_t offset, uint32_t *out)
{
  uint8_t bytes[4];

  if (!out || !MVM_ReadImageRange(ctx, offset, bytes, sizeof(bytes)))
  {
    return false;
  }

  *out = vm_read_u32_le(bytes);

  return true;
} /* End of MVM_lReadImageWord */

/**********************************************************************************************************************
 *  Name: vm_file_str
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static const char *vm_file_str(const VMGPContext *ctx, uint32_t off)
{
  const char *string = NULL;

  if (!ctx || !ctx->strtab || off >= ctx->header.string_size)
  {
    return NULL;
  }

  string = (const char *)(ctx->strtab + off);

  return string;
} /* End of vm_file_str */

/**********************************************************************************************************************
 *  Name: MVM_lAlignPoolSize
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Aligns one size value for runtime-pool planning.
 *********************************************************************************************************************/
static size_t MVM_lAlignPoolSize(size_t value)
{
  return (value + 3u) & ~(size_t)3u;
} /* End of MVM_lAlignPoolSize */

/**********************************************************************************************************************
 *  Name: MVM_lReadMemoryImage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Reads one byte range from a memory-backed VM image.
 *********************************************************************************************************************/
static int MVM_lReadMemoryImage(void *user, size_t offset, void *dst, size_t size)
{
  const uint8_t *image = (const uint8_t *)user;

  if (!image || !dst)
  {
    return -1;
  }

  memcpy(dst, image + offset, size);

  return 0;
} /* End of MVM_lReadMemoryImage */

/**********************************************************************************************************************
 *  END OF FILE MVM_VmgpLoader.c
 *********************************************************************************************************************/
