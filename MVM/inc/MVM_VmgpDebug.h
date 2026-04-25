/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_VmgpDebug.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public VMGP image structures and debug inspection helpers.
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

#define VMGP_MAGIC                                              "VMGP"
#define VMGP_POOL_SLOT_SIZE                                     (8U)

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/
/**
 * @brief Describes the fixed VMGP file header.
 */
typedef struct VMGPHeader
{
  char magic[4];        /**< File magic, expected to be `VMGP`. */
  uint16_t unknown1;    /**< Reserved header field. */
  uint16_t unknown2;    /**< Reserved header field. */
  uint16_t stack_words; /**< Initial stack size in 32-bit words. */
  uint8_t unknown3;     /**< Reserved header field. */
  uint8_t unknown4;     /**< Reserved header field. */
  uint32_t code_size;   /**< Code section size in bytes. */
  uint32_t data_size;   /**< Data section size in bytes. */
  uint32_t bss_size;    /**< BSS section size in bytes. */
  uint32_t res_size;    /**< Resource table size in bytes. */
  uint32_t unknown5;    /**< Reserved header field. */
  uint32_t pool_slots;  /**< Number of pool entries. */
  uint32_t string_size; /**< String table size in bytes. */
} VMGPHeader;

/**
 * @brief Describes one VMGP constant-pool entry.
 */
typedef struct VMGPPoolEntry
{
  uint8_t type;   /**< Pool entry type tag. */
  uint32_t aux24; /**< Auxiliary 24-bit payload stored in a 32-bit field. */
  uint32_t value; /**< Primary entry value or offset. */
} VMGPPoolEntry;

/**
 * @brief Describes one resource stored in the VM image.
 */
typedef struct VMGPResource
{
  uint32_t id;     /**< Resource identifier. */
  uint32_t offset; /**< Resource payload offset in VM memory. */
  uint32_t size;   /**< Resource payload size in bytes. */
} VMGPResource;

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Provides MVM_bVmgpParseHeader API.
 */
bool MVM_bVmgpParseHeader(MophunVM *vm);

/**
 * @brief Handles VMGP pool data.
 */
bool MVM_bVmgpLoadPool(MophunVM *vm);

/**
 * @brief Handles VMGP pool data.
 */
const VMGPPoolEntry *MVM_pudtVmgpGetPoolEntry(const MophunVM *vm, uint32_t pool_index_1based);

/**
 * @brief Handles VMGP pool data.
 */
const char *MVM_pudtVmgpPoolTypeName(uint8_t type);

/**
 * @brief Provides MVM_pudtVmgpGetImportName API.
 */
const char *MVM_pudtVmgpGetImportName(const MophunVM *vm, uint32_t pool_index_1based);

/**
 * @brief Handles VMGP pool data.
 */
size_t MVM_udtVmgpPoolSizeBytes(const VMGPHeader *header);

/**
 * @brief Provides MVM_vidVmgpDumpSummary API.
 */
void MVM_vidVmgpDumpSummary(const MophunVM *vm);

/**
 * @brief Provides MVM_vidVmgpDumpImports API.
 */
void MVM_vidVmgpDumpImports(const MophunVM *vm, uint32_t max_count);

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_VmgpDebug.h
 *********************************************************************************************************************/
