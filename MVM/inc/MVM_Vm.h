/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Vm.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public VM lifecycle, integration-config, bounded execution, and state-query API.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_VM_H
#define MVM_VM_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Provides MVM_udtGetStorageSize API.
 */
size_t MVM_udtGetStorageSize(void);

/**
 * @brief Provides MVM_udtGetStorageAlign API.
 */
size_t MVM_udtGetStorageAlign(void);

/**
 * @brief Provides MVM_pudtGetVmFromStorage API.
 */
MophunVM *MVM_pudtGetVmFromStorage(void *storage, size_t storage_size);

/**
 * @brief Initializes VM state with the default integration config.
 */
bool MVM_bInit(MophunVM *vm, const uint8_t *image, size_t image_size);

/**
 * @brief Initializes VM state using one explicit integration config object.
 */
bool MVM_bInitWithConfig(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const MVM_tstConfig *config);

/**
 * @brief Initializes VM state using the default config with overridden platform callbacks.
 */
bool MVM_bInitWithPlatform(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const MophunPlatform *platform);

/**
 * @brief Releases VM resources.
 */
void MVM_vidFree(MophunVM *vm);

/**
 * @brief Queries the static memory required for a VMGP image.
 */
bool MVM_bQueryMemoryRequirements(const uint8_t *image,
size_t image_size,
MVM_tstMemoryRequirements *requirements);

/**
 * @brief Executes at most one VM instruction without blocking.
 */
bool MVM_bStep(MophunVM *vm);

/**
 * @brief Executes up to the requested instruction budget.
 */
uint32_t MVM_u32RunSteps(MophunVM *vm, uint32_t max_steps);

/**
 * @brief Executes for up to the requested host time budget in milliseconds.
 */
uint32_t MVM_u32RunForTime(MophunVM *vm, uint32_t budget_ms);

/**
 * @brief Pauses VM execution.
 */
void MVM_vidPause(MophunVM *vm);

/**
 * @brief Moves the VM into a host-waiting state.
 */
void MVM_vidWait(MophunVM *vm);

/**
 * @brief Resumes VM execution after a pause or wait state.
 */
void MVM_vidResume(MophunVM *vm);

/**
 * @brief Requests immediate VM termination.
 */
void MVM_vidRequestExit(MophunVM *vm);

/**
 * @brief Returns the current execution state.
 */
MVM_tenuState MVM_tenuGetState(const MophunVM *vm);

/**
 * @brief Returns the last fatal execution error.
 */
MVM_tenuError MVM_tenuGetLastError(const MophunVM *vm);

/**
 * @brief Sets the no-progress step limit for the soft watchdog.
 */
void MVM_vidSetWatchdogLimit(MophunVM *vm, uint32_t no_progress_steps);

/**
 * @brief Returns the configured no-progress step limit for the soft watchdog.
 */
uint32_t MVM_u32GetWatchdogLimit(const MophunVM *vm);

/**
 * @brief Returns the total number of executed VM instructions.
 */
uint32_t MVM_u32GetExecutedSteps(const MophunVM *vm);

/**
 * @brief Returns the current VM program counter.
 */
uint32_t MVM_u32GetProgramCounter(const MophunVM *vm);

/**
 * @brief Returns the number of trace calls logged so far.
 */
uint32_t MVM_u32GetLoggedCalls(const MophunVM *vm);

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Vm.h
 *********************************************************************************************************************/
