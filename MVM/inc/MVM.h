/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public VM lifecycle, bounded execution, device-profile, and state-query API.
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
 * @brief Initializes VM state with the built-in integration config.
 */
MVM_tenuReturnCode MVM_enuInit(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const char *profile_name);

/**
 * @brief Releases VM resources.
 */
void MVM_vidFree(MophunVM *vm);

/**
 * @brief Queries the static memory required for a VMGP image.
 */
MVM_tenuReturnCode MVM_enuQueryMemoryRequirements(const uint8_t *image,
size_t image_size,
MVM_tstMemoryRequirements *requirements);

/**
 * @brief Returns the number of built-in device profiles.
 */
uint32_t MVM_u32GetDeviceProfileCount(void);

/**
 * @brief Returns one built-in device profile by zero-based index.
 */
const MophunDeviceProfile *MVM_pcdtGetDeviceProfile(uint32_t profile_index);

/**
 * @brief Finds one built-in device profile by name.
 */
const MophunDeviceProfile *MVM_pcdtFindDeviceProfileByName(const char *profile_name);

/**
 * @brief Executes at most one VM instruction without blocking.
 */
MVM_tenuReturnCode MVM_enuStep(MophunVM *vm);

/**
 * @brief Executes up to the requested instruction budget.
 */
MVM_tenuReturnCode MVM_enuRunSteps(MophunVM *vm, uint32_t max_steps, uint32_t *executed_steps);

/**
 * @brief Executes for up to the requested host time budget in milliseconds.
 */
MVM_tenuReturnCode MVM_enuRunForTime(MophunVM *vm, uint32_t budget_ms, uint32_t *executed_steps);

/**
 * @brief Pauses VM execution.
 */
MVM_tenuReturnCode MVM_enuPause(MophunVM *vm);

/**
 * @brief Moves the VM into a host-waiting state.
 */
MVM_tenuReturnCode MVM_enuWait(MophunVM *vm);

/**
 * @brief Resumes VM execution after a pause or wait state.
 */
MVM_tenuReturnCode MVM_enuResume(MophunVM *vm);

/**
 * @brief Requests immediate VM termination.
 */
MVM_tenuReturnCode MVM_enuRequestExit(MophunVM *vm);

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
MVM_tenuReturnCode MVM_enuSetWatchdogLimit(MophunVM *vm, uint32_t no_progress_steps);

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
 *  END OF FILE MVM.h
 *********************************************************************************************************************/
