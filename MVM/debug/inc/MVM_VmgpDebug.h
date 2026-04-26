/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_VmgpDebug.h
 *           Module:  MVM_Debug
 *           Target:  Portable C
 *      Description:  Internal decoded VMGP structures and debug dump helpers.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_VMGPDEBUG_H
#define MVM_VMGPDEBUG_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

#define VMGP_HEADER_SIZE    (40u)
#define VMGP_POOL_SLOT_SIZE (8u)
#define VMGP_MAGIC          "VMGP"

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Holds one decoded VMGP file header in host memory.
 *
 * The VMGP file format is parsed field-by-field and must not depend on native
 * compiler packing or on-disk `struct` layout.
 */
typedef struct VMGPHeader
{
  char magic[4];            /**< File magic, expected to be "VMGP". */
  uint16_t unknown1;        /**< Header field preserved for analysis. */
  uint16_t unknown2;        /**< Header field preserved for analysis. */
  uint16_t stack_words;     /**< Guest stack size in 32-bit words. */
  uint8_t unknown3;         /**< Header field preserved for analysis. */
  uint8_t unknown4;         /**< Header field preserved for analysis. */
  uint32_t code_size;       /**< Code section size in bytes. */
  uint32_t data_size;       /**< Data section size in bytes. */
  uint32_t bss_size;        /**< BSS section size in bytes. */
  uint32_t res_size;        /**< Resource section size in bytes. */
  uint32_t unknown5;        /**< Header field preserved for analysis. */
  uint32_t pool_slots;      /**< Number of 8-byte constant-pool slots. */
  uint32_t string_size;     /**< String table size in bytes. */
} VMGPHeader;

/**
 * @brief Holds one decoded constant-pool entry in host memory.
 *
 * This is an in-memory representation only; its native `sizeof(...)` is not
 * the same contract as one raw 8-byte pool slot stored in the file.
 */
typedef struct VMGPPoolEntry
{
  uint8_t type;     /**< Entry type tag from the VMGP pool. */
  uint32_t aux24;   /**< Decoded auxiliary 24-bit field. */
  uint32_t value;   /**< Decoded value or offset field. */
} VMGPPoolEntry;

/**
 * @brief Holds one decoded resource table entry in host memory.
 */
typedef struct VMGPResource
{
  uint32_t id;      /**< 1-based resource identifier visible to guest code. */
  uint32_t offset;  /**< Resource payload offset inside the resource section. */
  uint32_t size;    /**< Resource payload size in bytes. */
} VMGPResource;

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Dumps one decoded VMGP summary to the logger.
 */
void MVM_DumpVmgpSummary(const MpnVM_t *vm);

/**
 * @brief Dumps one decoded import list to the logger.
 */
void MVM_DumpVmgpImports(const MpnVM_t *vm, uint32_t max_count);

/**
 * @brief Returns one decoded pool size in bytes.
 */
size_t MVM_GetVmgpPoolSizeBytes(const VMGPHeader *header);

/**
 * @brief Returns one decoded pool entry by 1-based index.
 */
const VMGPPoolEntry *MVM_GetVmgpPoolEntry(const MpnVM_t *vm, uint32_t pool_index_1based);

/**
 * @brief Returns one decoded import name from the pool.
 */
const char *MVM_GetVmgpImportName(const MpnVM_t *vm, uint32_t pool_index_1based);

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_VmgpDebug.h
 *********************************************************************************************************************/
