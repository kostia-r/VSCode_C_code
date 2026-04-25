/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Log.c
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Shared VM logger implementation, compact event formatting, and default stdout sink.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Maps one log level to one compact severity character.
 */
static char MVM_lGetSeverityChar(MVM_LogLevel_t level);

/**
 * @brief Returns one compact text name for one structured VM event.
 */
static const char *MVM_lGetEventName(MVM_Event_t event);

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_LogMessage
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Writes one formatted log line through the configured platform hook.
 *********************************************************************************************************************/
void MVM_LogMessage(const VMGPContext *ctx,
                    MVM_LogLevel_t level,
                    const char *module,
                    const char *event,
                    const char *fmt,
                    ...)
{
  char buffer[MVM_LOG_BUFFER_SIZE];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  if (ctx && ctx->platform.log)
  {
    ctx->platform.log(ctx->platform.user, level, module, event, buffer);

    return;
  }

#if MVM_ENABLE_DEFAULT_LOGGER
  MVM_DefaultLog(NULL, level, module, event, buffer);
#else
  (void)level;
  (void)module;
  (void)event;
#endif
} /* End of MVM_LogMessage */

/**********************************************************************************************************************
 *  Name: MVM_LogEvent
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Emits one compact human-readable log line for one structured event.
 *********************************************************************************************************************/
void MVM_LogEvent(const VMGPContext *ctx, MVM_Event_t event, uint32_t arg0, uint32_t arg1)
{
  if (event == MVM_EVENT_IMPORT_CALL)
  {
    MVM_LOG_T(ctx,
              "event",
              "%s(arg0=%08X arg1=%08X)\n",
              MVM_lGetEventName(event),
              arg0,
              arg1);

    return;
  }

  MVM_LOG_D(ctx,
            "event",
            "%s(arg0=%08X arg1=%08X)\n",
            MVM_lGetEventName(event),
            arg0,
            arg1);
} /* End of MVM_LogEvent */

/**********************************************************************************************************************
 *  Name: MVM_DefaultLog
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Writes one compact default log line to stdout.
 *********************************************************************************************************************/
int MVM_DefaultLog(void *user,
                   MVM_LogLevel_t level,
                   const char *module,
                   const char *event,
                   const char *message)
{
  char line[MVM_LOG_BUFFER_SIZE + 32u];
  char severity = MVM_lGetSeverityChar(level);
  size_t message_len = 0u;

  (void)user;
  (void)module;
  (void)event;

  if (!message)
  {
    return 0;
  }

  message_len = strlen(message);

  while (message_len > 0u && ((message[message_len - 1u] == '\n') || (message[message_len - 1u] == '\r')))
  {
    message_len--;
  }

  snprintf(line,
           sizeof(line),
           "[%c][%s] %.*s\n",
           severity,
           MVM_CFG_LOG_CONTEXT_NAME,
           (int)message_len,
           message);

  return fputs(line, stdout);
} /* End of MVM_DefaultLog */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Name: MVM_lGetSeverityChar
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Maps one log level to one compact severity character.
 *********************************************************************************************************************/
static char MVM_lGetSeverityChar(MVM_LogLevel_t level)
{
  char severity = 'N';

  switch (level)
  {
    case MVM_LOG_LEVEL_ERROR:
    {
      severity = 'E';
      break;
    }

    case MVM_LOG_LEVEL_WARNING:
    {
      severity = 'W';
      break;
    }

    case MVM_LOG_LEVEL_INFO:
    {
      severity = 'I';
      break;
    }

    case MVM_LOG_LEVEL_DEBUG:
    {
      severity = 'D';
      break;
    }

    case MVM_LOG_LEVEL_TRACE:
    {
      severity = 'T';
      break;
    }

    default:
    {
      break;
    }
  }

  return severity;
} /* End of MVM_lGetSeverityChar */

/**********************************************************************************************************************
 *  Name: MVM_lGetEventName
 *  Upstream: N/A
 *  Synch/Asynch: Synchronous
 *  Reentrancy: No
 *  Parameters: See function signature.
 *  Returns: See function signature.
 *  Description: Returns one compact text name for one structured VM event.
 *********************************************************************************************************************/
static const char *MVM_lGetEventName(MVM_Event_t event)
{
  const char *name = "unknown";

  switch (event)
  {
    case MVM_EVENT_IMPORT_CALL:
    {
      name = "import-call";
      break;
    }

    case MVM_EVENT_MISSING_SYSCALL:
    {
      name = "missing-syscall";
      break;
    }

    case MVM_EVENT_INVALID_OPCODE:
    {
      name = "invalid-opcode";
      break;
    }

    case MVM_EVENT_MEMORY_OOB:
    {
      name = "memory-oob";
      break;
    }

    case MVM_EVENT_RESOURCE_OPENED:
    {
      name = "resource-opened";
      break;
    }

    case MVM_EVENT_RESOURCE_READ:
    {
      name = "resource-read";
      break;
    }

    case MVM_EVENT_FRAME_READY:
    {
      name = "frame-ready";
      break;
    }

    case MVM_EVENT_SOUND_REQUESTED:
    {
      name = "sound-requested";
      break;
    }

    case MVM_EVENT_VM_PAUSED:
    {
      name = "vm-paused";
      break;
    }

    case MVM_EVENT_VM_RESUMED:
    {
      name = "vm-resumed";
      break;
    }

    case MVM_EVENT_VM_WAITING:
    {
      name = "vm-waiting";
      break;
    }

    case MVM_EVENT_VM_EXITED:
    {
      name = "vm-exited";
      break;
    }

    case MVM_EVENT_VM_ERROR:
    {
      name = "vm-error";
      break;
    }

    default:
    {
      break;
    }
  }

  return name;
} /* End of MVM_lGetEventName */

/**********************************************************************************************************************
 *  END OF FILE MVM_Log.c
 *********************************************************************************************************************/
