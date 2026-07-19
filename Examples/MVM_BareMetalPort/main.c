/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  main.c
 *           Module:  MVM_BareMetalPort
 *           Target:  Generic bare metal
 *      Description:  Static-storage, bounded-execution reference integration loop.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Cfg.h"
#include "MVM_Port.h"
#include <stddef.h>

/**********************************************************************************************************************
 *  LOCAL MACROS
 *********************************************************************************************************************/

#define MVM_PORT_VM_STORAGE_SIZE                                (1024U * 1024U)
#define MVM_PORT_VM_STORAGE_ALIGN                               (64U)
#define MVM_PORT_STEP_BUDGET                                    (1000U)

/**********************************************************************************************************************
 *  LOCAL DATA
 *********************************************************************************************************************/

static _Alignas(MVM_PORT_VM_STORAGE_ALIGN) uint8_t MVM_lVmStorage[MVM_PORT_VM_STORAGE_SIZE];

/* Replace this build-check image with a decrypted MPN stored in flash. */
static const uint8_t MVM_lImageBytes[64U] = { 0U };
static const MVM_PortImage_t MVM_lImage = { MVM_lImageBytes, sizeof(MVM_lImageBytes) };

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

int main(void)
{
  MpnVM_t *vm;
  MpnImageSource_t imageSource;
#if !defined(MVM_PORT_BUILD_CHECK)
  MVM_MemReqs_t requirements;
  MVM_RetCode_t result;
  uint32_t executedSteps;
#endif

  if (MVM_GetStorageSize() > sizeof(MVM_lVmStorage) || MVM_GetStorageAlign() > MVM_PORT_VM_STORAGE_ALIGN)
  {
    return 1;
  }

  vm = MVM_GetVmFromStorage(MVM_lVmStorage, sizeof(MVM_lVmStorage));
  imageSource.user = (void *)&MVM_lImage;
  imageSource.image_size = MVM_lImage.size;
  imageSource.path = NULL;

#if defined(MVM_PORT_BUILD_CHECK)
  (void)vm;
  (void)imageSource;
  return 0;
#else
  result = MVM_QueryMemReqsFromSourceWithConfig(&imageSource, &MVM_Config, &requirements);
  if (!vm || result != MVM_OK || requirements.runtime_pool_bytes > MVM_Config.runtime_pool_size)
  {
    return 2;
  }

  result = MVM_InitFromSourceWithConfig(vm, &imageSource, "BOARD_128X160", &MVM_Config);
  if (result != MVM_OK)
  {
    return 3;
  }

  (void)MVM_SetFixedDateTime(vm, 2003U, 11U, 4U, 12U, 0U, 0U);

  for (;;)
  {
    executedSteps = 0U;
    result = MVM_RunSteps(vm, MVM_PORT_STEP_BUDGET, &executedSteps);
    if (result != MVM_OK || MVM_GetState(vm) == MVM_STATE_EXITED || MVM_GetState(vm) == MVM_STATE_ERROR)
    {
      break;
    }

    MVM_PortIdle();
  } /* End of loop */

  result = (MVM_GetState(vm) == MVM_STATE_ERROR) ? MVM_EXECUTION_ERROR : MVM_OK;
  MVM_Free(vm);
  return (result == MVM_OK) ? 0 : 4;
#endif
} /* End of main */
