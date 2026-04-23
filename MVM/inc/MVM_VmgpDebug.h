/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_VmgpDebug.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Mophun VM component header.
 *            Notes:  Structured according to project styling guidelines.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_VMGPDEBUG_H
#define MVM_VMGPDEBUG_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Vm.h"
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
typedef struct VMGPHeader
{
  char magic[4];
  uint16_t unknown1;
  uint16_t unknown2;
  uint16_t stack_words;
  uint8_t unknown3;
  uint8_t unknown4;
  uint32_t code_size;
  uint32_t data_size;
  uint32_t bss_size;
  uint32_t res_size;
  uint32_t unknown5;
  uint32_t pool_slots;
  uint32_t string_size;
} VMGPHeader;

typedef struct VMGPPoolEntry
{
  uint8_t type;
  uint32_t aux24;
  uint32_t value;
} VMGPPoolEntry;

typedef struct VMGPResource
{
  uint32_t id;
  uint32_t offset;
  uint32_t size;
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
