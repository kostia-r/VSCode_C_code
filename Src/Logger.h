#ifndef LOGGER_H
#define LOGGER_H

#include "MVM.h"
#include <stdarg.h>

/**
 * @brief Writes one complete preformatted OpenMophun log line.
 */
int Logger_Log(void *context, const char *message);

/**
 * @brief Formats and writes one timestamped desktop-platform log record.
 */
void Logger_VPrintf(uint32_t timestamp_ms,
                    MVM_LogLevel_t level,
                    const char *module,
                    const char *event,
                    const char *format,
                    va_list arguments);

#endif
