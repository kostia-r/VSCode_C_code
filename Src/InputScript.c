#include "InputScript.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SCRIPT_MAX_EVENTS        (512U)
#define INPUT_SCRIPT_LINE_SIZE         (160U)

#define MVM_KEY_UP                     (0x00000001U)
#define MVM_KEY_DOWN                   (0x00000002U)
#define MVM_KEY_LEFT                   (0x00000004U)
#define MVM_KEY_RIGHT                  (0x00000008U)
#define MVM_KEY_FIRE                   (0x00000010U)
#define MVM_KEY_SELECT                 (0x00000020U)
#define MVM_POINTER_DOWN               (0x00000040U)
#define MVM_POINTER_ALTDOWN            (0x00000080U)
#define MVM_KEY_FIRE2                  (0x00000100U)

typedef struct InputEvent
{
  uint32_t at_ms;
  uint32_t mask;
  int is_down;
} InputEvent;

struct InputScript
{
  InputEvent events[INPUT_SCRIPT_MAX_EVENTS];
  uint32_t event_count;
  uint32_t next_event;
  uint32_t button_mask;
  char last_error[128];
};

static void set_error(InputScript *script, const char *message)
{
  if (!script || !message)
  {
    return;
  }

  (void)snprintf(script->last_error, sizeof(script->last_error), "%s", message);
}

static char *trim_line(char *line)
{
  char *end;

  while (*line != '\0' && isspace((unsigned char)*line))
  {
    ++line;
  }

  end = line + strlen(line);
  while (end > line && isspace((unsigned char)end[-1]))
  {
    --end;
  }

  *end = '\0';

  return line;
}

static int token_equals(const char *a, const char *b)
{
  while (*a != '\0' && *b != '\0')
  {
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
    {
      return 0;
    }
    ++a;
    ++b;
  }

  return *a == '\0' && *b == '\0';
}

static uint32_t parse_button_mask(const char *name)
{
  if (token_equals(name, "UP"))
  {
    return MVM_KEY_UP;
  }
  if (token_equals(name, "DOWN"))
  {
    return MVM_KEY_DOWN;
  }
  if (token_equals(name, "LEFT"))
  {
    return MVM_KEY_LEFT;
  }
  if (token_equals(name, "RIGHT"))
  {
    return MVM_KEY_RIGHT;
  }
  if (token_equals(name, "FIRE"))
  {
    return MVM_KEY_FIRE;
  }
  if (token_equals(name, "FIRE2"))
  {
    return MVM_KEY_FIRE2;
  }
  if (token_equals(name, "SELECT"))
  {
    return MVM_KEY_SELECT;
  }
  if (token_equals(name, "POINTER"))
  {
    return MVM_POINTER_DOWN;
  }
  if (token_equals(name, "POINTER_ALT"))
  {
    return MVM_POINTER_ALTDOWN;
  }

  return 0u;
}

static int append_event(InputScript *script, uint32_t at_ms, uint32_t mask, int is_down)
{
  InputEvent *event;

  if (!script || mask == 0u)
  {
    return 0;
  }

  if (script->event_count >= INPUT_SCRIPT_MAX_EVENTS)
  {
    set_error(script, "input script has too many events");

    return 0;
  }

  event = &script->events[script->event_count++];
  event->at_ms = at_ms;
  event->mask = mask;
  event->is_down = is_down;

  return 1;
}

static int parse_uint_arg(const char *text, uint32_t *value)
{
  char *end;
  unsigned long parsed;

  if (!text || !*text || !value)
  {
    return 0;
  }

  parsed = strtoul(text, &end, 0);
  if (*end != '\0')
  {
    return 0;
  }

  *value = (uint32_t)parsed;

  return 1;
}

InputScript *InputScript_Load(const char *path)
{
  InputScript *script;
  FILE *file;
  char line[INPUT_SCRIPT_LINE_SIZE];
  uint32_t current_ms;
  uint32_t line_no;

  if (!path || !*path)
  {
    return NULL;
  }

  file = fopen(path, "r");
  if (!file)
  {
    return NULL;
  }

  script = (InputScript *)calloc(1u, sizeof(*script));
  if (!script)
  {
    fclose(file);

    return NULL;
  }

  current_ms = 0u;
  line_no = 0u;
  while (fgets(line, sizeof(line), file))
  {
    char *cursor;
    char *command;
    char *arg1;
    char *arg2;

    ++line_no;
    cursor = trim_line(line);
    if (*cursor == '\0' || *cursor == '#')
    {
      continue;
    }

    command = strtok(cursor, " \t");
    arg1 = strtok(NULL, " \t");
    arg2 = strtok(NULL, " \t");

    if (!command)
    {
      continue;
    }

    if (token_equals(command, "WAIT"))
    {
      uint32_t wait_ms;

      if (!parse_uint_arg(arg1, &wait_ms))
      {
        (void)snprintf(script->last_error, sizeof(script->last_error), "line %u: invalid WAIT", line_no);
        break;
      }

      current_ms += wait_ms;
    }
    else if (token_equals(command, "PRESS"))
    {
      uint32_t mask;
      uint32_t duration_ms;

      mask = parse_button_mask(arg1 ? arg1 : "");
      if (mask == 0u || !parse_uint_arg(arg2, &duration_ms))
      {
        (void)snprintf(script->last_error, sizeof(script->last_error), "line %u: invalid PRESS", line_no);
        break;
      }

      if (!append_event(script, current_ms, mask, 1) ||
          !append_event(script, current_ms + duration_ms, mask, 0))
      {
        break;
      }
      current_ms += duration_ms;
    }
    else if (token_equals(command, "DOWN") || token_equals(command, "UP"))
    {
      uint32_t mask;

      mask = parse_button_mask(arg1 ? arg1 : "");
      if (mask == 0u)
      {
        (void)snprintf(script->last_error, sizeof(script->last_error), "line %u: invalid button", line_no);
        break;
      }

      if (!append_event(script, current_ms, mask, token_equals(command, "DOWN")))
      {
        break;
      }
    }
    else
    {
      (void)snprintf(script->last_error, sizeof(script->last_error), "line %u: unknown command", line_no);
      break;
    }
  }

  fclose(file);
  if (script->last_error[0] != '\0')
  {
    return script;
  }

  set_error(script, "");

  return script;
}

void InputScript_Destroy(InputScript *script)
{
  free(script);
}

uint32_t InputScript_GetButtonMask(InputScript *script, uint32_t elapsed_ms)
{
  InputEvent *event;

  if (!script)
  {
    return 0u;
  }

  while (script->next_event < script->event_count &&
         script->events[script->next_event].at_ms <= elapsed_ms)
  {
    event = &script->events[script->next_event++];
    if (event->is_down)
    {
      script->button_mask |= event->mask;
    }
    else
    {
      script->button_mask &= ~event->mask;
    }
  }

  return script->button_mask;
}

const char *InputScript_GetLastError(const InputScript *script)
{
  if (!script)
  {
    return "input script is not loaded";
  }

  return script->last_error;
}
