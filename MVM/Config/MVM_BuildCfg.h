/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_BuildCfg.h
 *           Module:  MVM_Config
 *           Target:  Portable C
 *      Description:  Internal build-time switches used by the VM implementation and default integration layer.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_BUILD_CFG_H
#define MVM_BUILD_CFG_H

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

/*
 * Maximum number of simultaneously tracked VM resource streams.
 * This affects the size of the internal stream table in the VM context.
 */
#ifndef MVM_DEFAULT_MAX_STREAMS
#define MVM_DEFAULT_MAX_STREAMS                                 (16U)
#endif

/*
 * Enables the built-in fallback logger used by the default integration config.
 * Set to 0 for targets that require all logging to be provided by the host.
 */
#ifndef MVM_ENABLE_DEFAULT_LOGGER
#define MVM_ENABLE_DEFAULT_LOGGER                               (1U)
#endif

/*
 * Size of the temporary formatting buffer used by MVM_lLogf().
 * This limits the maximum formatted log message emitted in one call.
 */
#ifndef MVM_LOG_BUFFER_SIZE
#define MVM_LOG_BUFFER_SIZE                                     (256U)
#endif

/*
 * Compile-time maximum log level compiled into the VM.
 * 0=error, 1=warning, 2=info, 3=debug, 4=trace.
 */
#ifndef MVM_MAX_LOG_LEVEL
#define MVM_MAX_LOG_LEVEL                                       (3U)
#endif

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_BuildCfg.h
 *********************************************************************************************************************/
