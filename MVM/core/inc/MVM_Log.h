/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Log.h
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Shared VM logger interface, default sink, and level-gated log macros.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_LOG_H
#define MVM_LOG_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_BuildCfg.h"

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Writes one formatted log line through the configured platform hook.
 */
void MVM_LogMessage(const VMGPContext *ctx,
                    MVM_LogLevel_t level,
                    const char *module,
                    const char *event,
                    const char *fmt,
                    ...);

/**
 * @brief Emits one compact human-readable log line for one structured event.
 */
void MVM_LogEvent(const VMGPContext *ctx, MVM_Event_t event, uint32_t arg0, uint32_t arg1);

/**
 * @brief Writes one compact default log line to stdout.
 */
int MVM_DefaultLog(void *user,
                   MVM_LogLevel_t level,
                   const char *module,
                   const char *event,
                   const char *message);

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

#define MVM_LOG_RAW(ctx, level, event, fmt, ...) \
  MVM_LogMessage((ctx), (level), __FILE__, (event), (fmt), ##__VA_ARGS__)

#if (MVM_MAX_LOG_LEVEL >= 3U)
#define MVM_LOG_EVT(ctx, event_id, arg0, arg1) \
  MVM_LogEvent((ctx), (event_id), (arg0), (arg1))
#else
#define MVM_LOG_EVT(ctx, event_id, arg0, arg1) ((void)0)
#endif

#define MVM_LOG_E(ctx, event, fmt, ...) \
  MVM_LOG_RAW((ctx), MVM_LOG_LEVEL_ERROR, (event), (fmt), ##__VA_ARGS__)

#if (MVM_MAX_LOG_LEVEL >= 1U)
#define MVM_LOG_W(ctx, event, fmt, ...) \
  MVM_LOG_RAW((ctx), MVM_LOG_LEVEL_WARNING, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_LOG_W(ctx, event, fmt, ...) ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 2U)
#define MVM_LOG_I(ctx, event, fmt, ...) \
  MVM_LOG_RAW((ctx), MVM_LOG_LEVEL_INFO, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_LOG_I(ctx, event, fmt, ...) ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 3U)
#define MVM_LOG_D(ctx, event, fmt, ...) \
  MVM_LOG_RAW((ctx), MVM_LOG_LEVEL_DEBUG, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_LOG_D(ctx, event, fmt, ...) ((void)0)
#endif

#if (MVM_MAX_LOG_LEVEL >= 4U)
#define MVM_LOG_T(ctx, event, fmt, ...) \
  MVM_LOG_RAW((ctx), MVM_LOG_LEVEL_TRACE, (event), (fmt), ##__VA_ARGS__)
#else
#define MVM_LOG_T(ctx, event, fmt, ...) ((void)0)
#endif

#endif /* MVM_LOG_H */
