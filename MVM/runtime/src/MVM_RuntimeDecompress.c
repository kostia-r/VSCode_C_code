/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_RuntimeDecompress.c
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  Mophun VM component source.
 *            Notes:  Structured according to project styling guidelines.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <string.h>

/**********************************************************************************************************************
 *  LOCAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

typedef struct LZBitStream
{
  const uint8_t *data;
  uint32_t size;
  uint32_t bit_pos;
} LZBitStream;

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Reads an LZ resource header.
 */
static bool lz_read_header(const uint8_t *p, size_t remain, uint8_t *extended_offset_bits,
                           uint8_t *max_offset_bits, uint32_t *uncompressed_size, uint32_t *compressed_size);

/**
 * @brief Checks whether the LZ bit stream has readable data.
 */
static bool lz_bits_valid(const LZBitStream *bs);

/**
 * @brief Reads bits from an LZ bit stream.
 */
static uint32_t lz_read_bits(LZBitStream *bs, uint32_t count);

/**
 * @brief Expands compressed LZ resource payload.
 */
static uint32_t lz_decompress_content(const uint8_t *src, uint32_t src_size, uint8_t *dst,
                                      uint32_t dst_size, uint8_t extended_offset_bits, uint8_t max_offset_bits);

/**
 * @brief Finds an active VM resource stream by handle.
 */
static VMGPStream *find_stream(VMGPContext *ctx, uint32_t handle);

/**
 * @brief Handles the vDecompHdr runtime import.
 */
static bool handle_decomp_hdr(VMGPContext *ctx);

/**
 * @brief Handles the vDecompress runtime import.
 */
static bool handle_decompress(VMGPContext *ctx);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_bRuntimeHandleDecompress
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Handles runtime syscall flow.
 *********************************************************************************************************************/
bool MVM_bRuntimeHandleDecompress(VMGPContext *ctx, const char *name)
{
  if (strcmp(name, "vDecompHdr") == 0)
  {
    return handle_decomp_hdr(ctx);
  }

  if (strcmp(name, "vDecompress") == 0)
  {
    return handle_decompress(ctx);
  }

  return false;
} /* End of MVM_bRuntimeHandleDecompress */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: lz_read_header
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static bool lz_read_header(const uint8_t *p, size_t remain, uint8_t *extended_offset_bits,
                           uint8_t *max_offset_bits, uint32_t *uncompressed_size, uint32_t *compressed_size)
{
  uint32_t raw_size;
  uint32_t packed_size;

  if (!p || remain < 22 || p[0] != 'L' || p[1] != 'Z')
  {
    return false;
  }

  raw_size = vm_read_u32_le(p + 4);
  packed_size = vm_read_u32_le(p + 8);

  if (extended_offset_bits)
  {
    *extended_offset_bits = p[3];
  }

  if (max_offset_bits)
  {
    *max_offset_bits = p[2];
  }

  if (compressed_size)
  {
    *compressed_size = packed_size;
  }

  /* Observed on this T310 game: these entries allocate packed_size - 1. */

  if (raw_size == 0x200u && packed_size > 1u && packed_size < raw_size)
  {
    raw_size = packed_size - 1u;
  }

  if (uncompressed_size)
  {
    *uncompressed_size = raw_size;
  }

  return true;
} /* End of lz_read_header */

/**********************************************************************************************************************
 *  Name: lz_bits_valid
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static bool lz_bits_valid(const LZBitStream *bs)
{
  return bs && bs->bit_pos < bs->size * 8u;
} /* End of lz_bits_valid */

/**********************************************************************************************************************
 *  Name: lz_read_bits
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static uint32_t lz_read_bits(LZBitStream *bs, uint32_t count)
{
  uint32_t result = 0;
  uint32_t i;

  for (i = 0; i < count; ++i)
  {
    result <<= 1;

    if (lz_bits_valid(bs))
    {
      uint32_t byte_index = bs->bit_pos >> 3;
      uint32_t bit_index = 7u - (bs->bit_pos & 7u);
      result |= (uint32_t)((bs->data[byte_index] >> bit_index) & 1u);
      bs->bit_pos++;
    }
  } /* End of loop */

  return result;
} /* End of lz_read_bits */

/**********************************************************************************************************************
 *  Name: lz_decompress_content
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static uint32_t lz_decompress_content(const uint8_t *src,
                                      uint32_t src_size,
                                      uint8_t *dst,
                                      uint32_t dst_size,
                                      uint8_t extended_offset_bits,
                                      uint8_t max_offset_bits)
{
  LZBitStream bs;
  uint32_t dst_pos = 0;

  bs.data = src;
  bs.size = src_size;
  bs.bit_pos = 0;

  while (dst_pos < dst_size && lz_bits_valid(&bs))
  {
    if (lz_read_bits(&bs, 1) == 1)
    {
      uint32_t v2 = 0;
      uint32_t copy_len = 2;
      uint32_t back_offset;
      uint32_t i;

      while (v2 < max_offset_bits && lz_read_bits(&bs, 1) == 1)
      {
        v2++;
      } /* End of loop */

      if (v2 != 0)
      {
        copy_len = (lz_read_bits(&bs, v2) | (1u << v2)) + 1u;
      }

      if (copy_len == 2)
      {
        back_offset = lz_read_bits(&bs, 8) + 2u;
      }
      else
      {
        back_offset = lz_read_bits(&bs, extended_offset_bits) + copy_len;
      }

      for (i = 0; i < copy_len && dst_pos < dst_size; ++i)
      {
        uint32_t from = (back_offset <= dst_pos) ? (dst_pos - back_offset) : 0u;
        dst[dst_pos] = dst[from];
        dst_pos++;
      } /* End of loop */
    }
    else
    {
      dst[dst_pos++] = (uint8_t)(lz_read_bits(&bs, 8) & 0xFFu);
    }
  } /* End of loop */

  return dst_pos;
} /* End of lz_decompress_content */

/**********************************************************************************************************************
 *  Name: find_stream
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static VMGPStream *find_stream(VMGPContext *ctx, uint32_t handle)
{
  uint32_t i;

  for (i = 0; i < VMGP_MAX_STREAMS; ++i)
  {

    if (ctx->streams[i].used && ctx->streams[i].handle == handle)
    {
      return &ctx->streams[i];
    }
  } /* End of loop */

  return NULL;
} /* End of find_stream */

/**********************************************************************************************************************
 *  Name: handle_decomp_hdr
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static bool handle_decomp_hdr(VMGPContext *ctx)
{
  uint32_t info = ctx->regs[VM_REG_P0];
  uint32_t hdr = ctx->regs[VM_REG_P1];
  uint8_t extended_offset_bits = 0;
  uint8_t max_offset_bits = 0;
  uint32_t uncompressed_size = 0;
  uint32_t compressed_size = 0;

  if (!MVM_LbRuntimeMemRangeOk(ctx, hdr, 22) ||
      !lz_read_header(ctx->mem + hdr,
                      ctx->mem_size - hdr,
                      &extended_offset_bits,
                      &max_offset_bits,
                      &uncompressed_size,
                      &compressed_size))
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
    return true;
  }

  if (info != 0 && MVM_LbRuntimeMemRangeOk(ctx, info, 20))
  {
    ctx->mem[info + 0] = 0;
    ctx->mem[info + 1] = 0;
    vm_write_u16_le(ctx->mem + info + 2, 0x1234);
    vm_write_u16_le(ctx->mem + info + 4, 0);
    vm_write_u16_le(ctx->mem + info + 6, 0);
    vm_write_u32_le(ctx->mem + info + 8, compressed_size);
    vm_write_u32_le(ctx->mem + info + 12, uncompressed_size);
    vm_write_u32_le(ctx->mem + info + 16, 0);
  }

  ctx->regs[VM_REG_R0] = uncompressed_size;
  return true;
} /* End of handle_decomp_hdr */

/**********************************************************************************************************************
 *  Name: handle_decompress
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Provides VM component logic.
 *********************************************************************************************************************/
static bool handle_decompress(VMGPContext *ctx)
{
  uint32_t src = ctx->regs[VM_REG_P0];
  uint32_t dst = ctx->regs[VM_REG_P1];
  uint32_t stream_handle = ctx->regs[VM_REG_P2];
  VMGPStream *s = NULL;
  const uint8_t *base = NULL;
  uint32_t available = 0;
  uint32_t stream_base_pos = 0;
  uint8_t extended_offset_bits = 0;
  uint8_t max_offset_bits = 0;
  uint32_t out_size = 0;
  uint32_t packed_size = 0;
  uint32_t produced = 0;

  if (src != 0)
  {

    if (!MVM_LbRuntimeMemRangeOk(ctx, src, 22))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    base = ctx->mem + src;
    available = (uint32_t)(ctx->mem_size - src);
  }

  else
  {
    s = find_stream(ctx, stream_handle);

    if (!s || !MVM_LbRuntimeMemRangeOk(ctx, s->base + s->pos, 22))
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }
    stream_base_pos = s->pos;
    base = ctx->mem + s->base + s->pos;
    available = s->size - s->pos;
  }

  if (!lz_read_header(base, available, &extended_offset_bits, &max_offset_bits, &out_size, &packed_size))
  {
    uint32_t copy_size = available;
    uint32_t dst_limit;

    if (dst >= ctx->mem_size)
    {
      ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
      return true;
    }

    dst_limit = ctx->mem_size - dst;

    if (dst < ctx->heap_cur)
    {
      uint32_t heap_limit = ctx->heap_cur - dst;

      if (heap_limit < dst_limit)
      {
        dst_limit = heap_limit;
      }
    }

    if (copy_size > dst_limit)
    {
      copy_size = dst_limit;
    }

    if (copy_size > 0)
    {
      memcpy(ctx->mem + dst, base, copy_size);
    }

    if (s)
    {
      s->pos = stream_base_pos + copy_size;

      if (s->pos > s->size)
      {
        s->pos = s->size;
      }
    }

    ctx->regs[VM_REG_R0] = copy_size;
    return true;
  }

  if (!MVM_LbRuntimeMemRangeOk(ctx, dst, out_size))
  {
    ctx->regs[VM_REG_R0] = 0xFFFFFFFFu;
    return true;
  }

  if (packed_size > available - 22u)
  {
    packed_size = available - 22u;
  }

  produced = lz_decompress_content(base + 22u,
                                   packed_size,
                                   ctx->mem + dst,
                                   out_size,
                                   extended_offset_bits,
                                   max_offset_bits);

  if (s)
  {
    uint32_t consumed = 22u + packed_size;
    s->pos = stream_base_pos + consumed;

    if (s->pos > s->size)
    {
      s->pos = s->size;
    }
  }

  ctx->regs[VM_REG_R0] = produced;
  return true;
} /* End of handle_decompress */

/**********************************************************************************************************************
 *  END OF FILE MVM_RuntimeDecompress.c
 *********************************************************************************************************************/
