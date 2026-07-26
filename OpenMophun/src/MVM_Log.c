/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  OpenMophun
 *             File:  MVM_Log.c
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Shared VM log-line and compact event formatting.
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
 * @brief Returns one compact text name for one structured VM event.
 */
#if (MVM_COMPILED_LOG_LEVEL >= 3U)
static const char *MVM_lGetEventName(MVM_Event_t event);
#endif

#if (MVM_COMPILED_LOG_LEVEL >= 1U)
static char MVM_lGetSeverityChar(MVM_LogLevel_t level);
#endif

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
                    const char *fmt,
                    ...)
{
#if (MVM_COMPILED_LOG_LEVEL >= 1U)
  char message[MVM_LOG_BUFFER_SIZE];
  char line[MVM_LOG_BUFFER_SIZE + 32U];
  char severity;
  size_t message_length;
  uint32_t timestamp_ms;
  va_list ap;

  if (!ctx || !ctx->drivers.log || ctx->log_level == MVM_LOG_LEVEL_OFF || level > ctx->log_level)
  {
    return;
  }

  va_start(ap, fmt);
  (void)vsnprintf(message, sizeof(message), fmt, ap);
  va_end(ap);

  timestamp_ms = 0U;
  if (ctx->drivers.get_ticks_ms)
  {
    timestamp_ms = ctx->drivers.get_ticks_ms(ctx->drivers.context) - ctx->log_start_ms;
  }

  severity = MVM_lGetSeverityChar(level);
  message_length = strlen(message);

  while (message_length > 0U &&
         (message[message_length - 1U] == '\n' || message[message_length - 1U] == '\r'))
  {
    --message_length;
  }

  (void)snprintf(line,
                 sizeof(line),
                 "[%5lu.%03lu] [%c][MVM] %.*s\n",
                 (unsigned long)(timestamp_ms / 1000U),
                 (unsigned long)(timestamp_ms % 1000U),
                 severity,
                 (int)message_length,
                 message);
  ctx->drivers.log(ctx->drivers.context, line);
#else
  (void)ctx;
  (void)level;
  (void)fmt;
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
#if (MVM_COMPILED_LOG_LEVEL >= 3U)
  if (event == MVM_EVENT_DATA_CERT_CHECKED ||
      event == MVM_EVENT_LICENSE_EXPIRED ||
      event == MVM_EVENT_DEVICE_UNSUPPORTED ||
      event == MVM_EVENT_SIDECAR_MISSING ||
      event == MVM_EVENT_SYSTEM_MESSAGE)
  {
    MVM_LOG_I(ctx,
              "event",
              "%s(arg0=%08X arg1=%08X)\n",
              MVM_lGetEventName(event),
              arg0,
              arg1);

    return;
  }

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
#else
  (void)ctx;
  (void)event;
  (void)arg0;
  (void)arg1;
#endif
} /* End of MVM_LogEvent */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

#if (MVM_COMPILED_LOG_LEVEL >= 1U)
static char MVM_lGetSeverityChar(MVM_LogLevel_t level)
{
  char severity;

  severity = '?';

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
}
#endif

#if (MVM_COMPILED_LOG_LEVEL >= 3U)
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

    case MVM_EVENT_DATA_CERT_CHECKED:
    {
      name = "data-cert-checked";
      break;
    }

    case MVM_EVENT_LICENSE_EXPIRED:
    {
      name = "license-expired";
      break;
    }

    case MVM_EVENT_DEVICE_UNSUPPORTED:
    {
      name = "device-unsupported";
      break;
    }

    case MVM_EVENT_SIDECAR_MISSING:
    {
      name = "sidecar-missing";
      break;
    }

    case MVM_EVENT_SYSTEM_MESSAGE:
    {
      name = "system-message";
      break;
    }

    default:
    {
      break;
    }
  }

  return name;
} /* End of MVM_lGetEventName */
#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Log.c
 *********************************************************************************************************************/
