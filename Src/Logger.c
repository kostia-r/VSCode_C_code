#include "Logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int Logger_Log(void *context, const char *message)
{
  (void)context;

  if (!message)
  {
    return 0;
  }

  return fputs(message, stdout);
}

void Logger_VPrintf(uint32_t timestamp_ms,
                    MVM_LogLevel_t level,
                    const char *module,
                    const char *event,
                    const char *format,
                    va_list arguments)
{
  char message[256];
  char line[288];
  char severity;
  size_t message_length;

  if (!format)
  {
    return;
  }

  (void)event;

  (void)vsnprintf(message, sizeof(message), format, arguments);
  severity = level == MVM_LOG_LEVEL_ERROR ? 'E' :
             level == MVM_LOG_LEVEL_WARNING ? 'W' :
             level == MVM_LOG_LEVEL_INFO ? 'I' :
             level == MVM_LOG_LEVEL_DEBUG ? 'D' :
             level == MVM_LOG_LEVEL_TRACE ? 'T' : '?';
  message_length = strlen(message);
  (void)snprintf(line,
                 sizeof(line),
                 "[%5u.%03u] [%c][%s] %.*s\n",
                 timestamp_ms / 1000U,
                 timestamp_ms % 1000U,
                 severity,
                 module ? module : "HOST",
                 (int)message_length,
                 message);
  (void)Logger_Log(NULL, line);
}
