#include "SdlBackend.h"

#include "MidiRenderer.h"
#include "MVM_Render.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH                       (260)
#endif

#define INPUT_REPEAT_DELAY_MS          (180U)
#define INPUT_REPEAT_INTERVAL_MS       (90U)
#define MVM_KEY_UP                     (0x00000001U)
#define MVM_KEY_DOWN                   (0x00000002U)
#define MVM_KEY_LEFT                   (0x00000004U)
#define MVM_KEY_RIGHT                  (0x00000008U)
#define MVM_KEY_FIRE                   (0x00000010U)
#define MVM_KEY_SELECT                 (0x00000020U)
#define MVM_POINTER_DOWN               (0x00000040U)
#define MVM_POINTER_ALTDOWN            (0x00000080U)
#define MVM_KEY_FIRE2                  (0x00000100U)
#define SDL_BACKEND_BUTTON_COUNT       (9U)
#define SOUND_RESOURCE_TYPE_MASK       (0x000000FFU)
#define SOUND_TYPE_BEEP                (0x00000000U)
#define SOUND_TYPE_MIDI                (0x00000002U)
#define SOUND_TYPE_AMR                 (0x00000003U)
#define SOUND_FLAG_LOOP                (0x00000100U)
#define SOUND_FLAG_STREAM              (0x00000200U)
#define SOUND_FLAG_STOP                (0x00000400U)
#define DEFAULT_SOUNDFONT_PATH         "Assets/DefaultSfBank.bytes"
#define RECORDING_FPS                  (30U)

#ifdef DEBUG
#define SDL_BACKEND_LOG_D(...)         printf(__VA_ARGS__)
#else
#define SDL_BACKEND_LOG_D(...)         ((void)0)
#endif

/*
 * Desktop key mapping aligned with the official Mophun SDK emulator:
 * - Up Arrow    -> KEY_UP
 * - Down Arrow  -> KEY_DOWN
 * - Left Arrow  -> KEY_LEFT
 * - Right Arrow -> KEY_RIGHT
 * - Left/Right Shift/Ctrl -> KEY_FIRE
 * - Backspace/Enter  -> KEY_SELECT
 * - Space / Keypad Enter -> KEY_FIRE2
 * - Numeric keypad 1/3/7/9 -> diagonal direction combinations
 * - Numeric keypad 2/4/6/8 -> down/left/right/up
 */

struct SdlBackend
{
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *framebuffer;
  SDL_Texture *displaybuffer;
  SDL_AudioDeviceID audio_device;
  SDL_AudioSpec audio_spec;
  MidiRenderer *midi_renderer;
  uint32_t width;
  uint32_t height;
  uint32_t raw_button_state;
  uint32_t pulse_button_state;
  uint32_t synthetic_button_state;
  uint32_t next_repeat_ms[SDL_BACKEND_BUTTON_COUNT];
  uint32_t last_frame_serial;
  uint32_t last_clear_serial;
  uint32_t last_draw_command_count;
  FILE *record_frame_file;
  FILE *record_audio_file;
  FILE *record_meta_file;
  uint32_t record_start_ms;
  uint32_t record_next_frame_ms;
  uint32_t record_frame_count;
  uint32_t record_audio_sample_count;
  uint32_t record_audio_bytes;
#ifdef _WIN32
  char midi_alias[32];
  char midi_path[MAX_PATH];
  uint32_t midi_serial;
#endif
};

static const uint32_t MVM_lSdlButtonMasks[SDL_BACKEND_BUTTON_COUNT] =
{
  MVM_KEY_UP,
  MVM_KEY_DOWN,
  MVM_KEY_LEFT,
  MVM_KEY_RIGHT,
  MVM_KEY_FIRE,
  MVM_KEY_SELECT,
  MVM_POINTER_DOWN,
  MVM_POINTER_ALTDOWN,
  MVM_KEY_FIRE2
};

static uint32_t sdl_platform_get_ticks_ms(void *user);

static int build_record_path(char *dst, size_t dst_size, const char *dir, const char *name)
{
  size_t dir_len;
  const char *separator;

  if (!dst || dst_size == 0u || !dir || !name)
  {
    return 0;
  }

  dir_len = strlen(dir);
  separator = (dir_len > 0u && (dir[dir_len - 1u] == '\\' || dir[dir_len - 1u] == '/')) ? "" : "\\";

  return snprintf(dst, dst_size, "%s%s%s", dir, separator, name) > 0;
}

static void write_wav_u16(FILE *file, uint16_t value)
{
  fputc((int)(value & 0xFFu), file);
  fputc((int)((value >> 8) & 0xFFu), file);
}

static void write_wav_u32(FILE *file, uint32_t value)
{
  fputc((int)(value & 0xFFu), file);
  fputc((int)((value >> 8) & 0xFFu), file);
  fputc((int)((value >> 16) & 0xFFu), file);
  fputc((int)((value >> 24) & 0xFFu), file);
}

static void write_wav_header(FILE *file, uint32_t sample_rate, uint32_t data_bytes)
{
  if (!file)
  {
    return;
  }

  fwrite("RIFF", 1u, 4u, file);
  write_wav_u32(file, 36u + data_bytes);
  fwrite("WAVE", 1u, 4u, file);
  fwrite("fmt ", 1u, 4u, file);
  write_wav_u32(file, 16u);
  write_wav_u16(file, 1u);
  write_wav_u16(file, 1u);
  write_wav_u32(file, sample_rate);
  write_wav_u32(file, sample_rate * 2u);
  write_wav_u16(file, 2u);
  write_wav_u16(file, 16u);
  fwrite("data", 1u, 4u, file);
  write_wav_u32(file, data_bytes);
}

static uint32_t get_record_audio_sample_rate(SdlBackend *backend)
{
  if (backend && backend->audio_spec.freq > 0)
  {
    return (uint32_t)backend->audio_spec.freq;
  }

  return 22050u;
}

static void record_audio_silence(SdlBackend *backend, uint32_t sample_count)
{
  int16_t zeros[512];

  if (!backend || !backend->record_audio_file || sample_count == 0u)
  {
    return;
  }

  memset(zeros, 0, sizeof(zeros));
  while (sample_count != 0u)
  {
    uint32_t chunk;

    chunk = sample_count > 512u ? 512u : sample_count;
    fwrite(zeros, sizeof(zeros[0]), chunk, backend->record_audio_file);
    backend->record_audio_sample_count += chunk;
    backend->record_audio_bytes += chunk * (uint32_t)sizeof(zeros[0]);
    sample_count -= chunk;
  }
}

static void record_audio_pad_to_ms(SdlBackend *backend, uint32_t elapsed_ms)
{
  uint32_t sample_rate;
  uint32_t target_samples;

  if (!backend || !backend->record_audio_file)
  {
    return;
  }

  sample_rate = get_record_audio_sample_rate(backend);
  target_samples = (uint32_t)(((uint64_t)sample_rate * elapsed_ms) / 1000u);
  if (target_samples > backend->record_audio_sample_count)
  {
    record_audio_silence(backend, target_samples - backend->record_audio_sample_count);
  }
}

static void record_audio_samples(SdlBackend *backend, const int16_t *samples, uint32_t sample_count)
{
  uint32_t now_ms;

  if (!backend || !backend->record_audio_file || !samples || sample_count == 0u)
  {
    return;
  }

  now_ms = sdl_platform_get_ticks_ms(backend);
  if (now_ms >= backend->record_start_ms)
  {
    record_audio_pad_to_ms(backend, now_ms - backend->record_start_ms);
  }

  fwrite(samples, sizeof(*samples), sample_count, backend->record_audio_file);
  backend->record_audio_sample_count += sample_count;
  backend->record_audio_bytes += sample_count * (uint32_t)sizeof(*samples);
}

static void record_current_frame(SdlBackend *backend)
{
  void *pixels;
  size_t frame_bytes;
  uint32_t now_ms;

  if (!backend || !backend->record_frame_file || !backend->renderer)
  {
    return;
  }

  now_ms = sdl_platform_get_ticks_ms(backend);
  if (now_ms < backend->record_start_ms)
  {
    return;
  }

  if ((now_ms - backend->record_start_ms) < backend->record_next_frame_ms)
  {
    return;
  }

  frame_bytes = (size_t)backend->width * (size_t)backend->height * 4u;
  pixels = malloc(frame_bytes);
  if (!pixels)
  {
    return;
  }

  if (SDL_RenderReadPixels(backend->renderer,
                           NULL,
                           SDL_PIXELFORMAT_ABGR8888,
                           pixels,
                           (int)(backend->width * 4u)) == 0)
  {
    do
    {
      fwrite(pixels, 1u, frame_bytes, backend->record_frame_file);
      ++backend->record_frame_count;
      backend->record_next_frame_ms = (backend->record_frame_count * 1000u) / RECORDING_FPS;
    } while ((now_ms - backend->record_start_ms) >= backend->record_next_frame_ms);
  }

  free(pixels);
}

static void decode_guest_color(uint32_t color, uint8_t *red, uint8_t *green, uint8_t *blue)
{
  if ((color & 0x80000000u) != 0u || color > 0xFFu)
  {
    *red = (uint8_t)((((color & 0xFFFFu) >> 10) & 0x1Fu) * 255u / 31u);
    *green = (uint8_t)((((color & 0xFFFFu) >> 5) & 0x1Fu) * 255u / 31u);
    *blue = (uint8_t)(((color & 0xFFFFu) & 0x1Fu) * 255u / 31u);
  }
  else
  {
    *red = (uint8_t)(((color >> 5) & 0x07u) * 255u / 7u);
    *green = (uint8_t)(((color >> 2) & 0x07u) * 255u / 7u);
    *blue = (uint8_t)((color & 0x03u) * 255u / 3u);
  }
}

static void set_renderer_guest_color(SDL_Renderer *renderer, uint32_t color)
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  if (!renderer)
  {
    return;
  }

  decode_guest_color(color, &red, &green, &blue);
  SDL_SetRenderDrawColor(renderer, red, green, blue, 255u);
}

static void sdl_render_set_clip(void *user, int enabled, int32_t x, int32_t y, int32_t width, int32_t height)
{
  SdlBackend *backend;
  SDL_Rect clip_rect;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  if (enabled)
  {
    clip_rect.x = (int)x;
    clip_rect.y = (int)y;
    clip_rect.w = (int)width;
    clip_rect.h = (int)height;
    SDL_RenderSetClipRect(backend->renderer, &clip_rect);
  }
  else
  {
    SDL_RenderSetClipRect(backend->renderer, NULL);
  }
}

static void sdl_render_draw_point(void *user, int32_t x, int32_t y, uint8_t red, uint8_t green, uint8_t blue)
{
  SdlBackend *backend;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  SDL_SetRenderDrawColor(backend->renderer, red, green, blue, 255u);
  SDL_RenderDrawPoint(backend->renderer, x, y);
}

static void sdl_render_draw_line(void *user,
                                 int32_t x0,
                                 int32_t y0,
                                 int32_t x1,
                                 int32_t y1,
                                 uint8_t red,
                                 uint8_t green,
                                 uint8_t blue)
{
  SdlBackend *backend;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  SDL_SetRenderDrawColor(backend->renderer, red, green, blue, 255u);
  SDL_RenderDrawLine(backend->renderer, x0, y0, x1, y1);
}

static void sdl_render_fill_rect(void *user,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 uint8_t red,
                                 uint8_t green,
                                 uint8_t blue)
{
  SdlBackend *backend;
  SDL_Rect rect;

  backend = (SdlBackend *)user;
  if (!backend || !backend->renderer)
  {
    return;
  }

  rect.x = (int)x;
  rect.y = (int)y;
  rect.w = (int)width;
  rect.h = (int)height;
  SDL_SetRenderDrawColor(backend->renderer, red, green, blue, 255u);
  SDL_RenderFillRect(backend->renderer, &rect);
}

static uint32_t sdl_platform_get_ticks_ms(void *user)
{
  (void)user;

  return (uint32_t)SDL_GetTicks();
}

#ifdef _WIN32
static void stop_backend_midi(SdlBackend *backend)
{
  char command[128];

  if (!backend || backend->midi_alias[0] == '\0')
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "stop %s", backend->midi_alias);
  (void)mciSendStringA(command, NULL, 0, NULL);
  (void)snprintf(command, sizeof(command), "close %s", backend->midi_alias);
  (void)mciSendStringA(command, NULL, 0, NULL);

  backend->midi_alias[0] = '\0';
  if (backend->midi_path[0] != '\0')
  {
    DeleteFileA(backend->midi_path);
    backend->midi_path[0] = '\0';
  }
}

static int play_backend_midi(SdlBackend *backend, const uint8_t *data, uint32_t length, int loop)
{
  char temp_path[MAX_PATH];
  char temp_name[40];
  char command[MAX_PATH + 96];
  DWORD written;
  HANDLE file;
  MCIERROR error;

  if (!backend || !data || length == 0u)
  {
    return 0;
  }

  stop_backend_midi(backend);

  if (GetTempPathA((DWORD)sizeof(temp_path), temp_path) == 0u)
  {
    return 0;
  }

  ++backend->midi_serial;
  (void)snprintf(temp_name,
                 sizeof(temp_name),
                 "mvm_%08X_%08X.mid",
                 (unsigned int)GetCurrentProcessId(),
                 (unsigned int)backend->midi_serial);
  if (strlen(temp_path) + strlen(temp_name) >= sizeof(backend->midi_path))
  {
    backend->midi_alias[0] = '\0';
    backend->midi_path[0] = '\0';
    return 0;
  }
  (void)strcpy(backend->midi_path, temp_path);
  (void)strcat(backend->midi_path, temp_name);
  (void)snprintf(backend->midi_alias, sizeof(backend->midi_alias), "mvm%08X", (unsigned int)backend->midi_serial);

  file = CreateFileA(backend->midi_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
  if (file == INVALID_HANDLE_VALUE)
  {
    backend->midi_alias[0] = '\0';
    backend->midi_path[0] = '\0';
    return 0;
  }

  if (!WriteFile(file, data, length, &written, NULL) || written != length)
  {
    CloseHandle(file);
    DeleteFileA(backend->midi_path);
    backend->midi_alias[0] = '\0';
    backend->midi_path[0] = '\0';
    return 0;
  }
  CloseHandle(file);

  (void)snprintf(command, sizeof(command), "open \"%s\" type sequencer alias %s", backend->midi_path, backend->midi_alias);
  error = mciSendStringA(command, NULL, 0, NULL);
  if (error != 0u)
  {
    DeleteFileA(backend->midi_path);
    backend->midi_alias[0] = '\0';
    backend->midi_path[0] = '\0';
    return 0;
  }

  (void)snprintf(command, sizeof(command), "play %s%s", backend->midi_alias, loop ? " repeat" : "");
  error = mciSendStringA(command, NULL, 0, NULL);
  if (error != 0u)
  {
    stop_backend_midi(backend);
    return 0;
  }

  return 1;
}
#else
static void stop_backend_midi(SdlBackend *backend)
{
  (void)backend;
}

static int play_backend_midi(SdlBackend *backend, const uint8_t *data, uint32_t length, int loop)
{
  (void)backend;
  (void)data;
  (void)length;
  (void)loop;

  return 0;
}
#endif

static void queue_square_tone(SdlBackend *backend, uint32_t frequency, uint32_t duration_ms, uint32_t volume)
{
  int16_t *samples;
  uint32_t sample_rate;
  uint32_t sample_count;
  uint32_t period;
  uint32_t index;
  int16_t amplitude;

  if (!backend || backend->audio_device == 0u || frequency == 0u)
  {
    return;
  }

  sample_rate = backend->audio_spec.freq > 0 ? (uint32_t)backend->audio_spec.freq : 22050u;
  if (duration_ms < 40u)
  {
    duration_ms = 40u;
  }
  if (duration_ms > 220u)
  {
    duration_ms = 220u;
  }

  sample_count = (sample_rate * duration_ms) / 1000u;
  if (sample_count == 0u)
  {
    return;
  }

  samples = (int16_t *)malloc(sample_count * sizeof(*samples));
  if (!samples)
  {
    return;
  }

  period = sample_rate / frequency;
  if (period == 0u)
  {
    period = 1u;
  }

  if (volume > 255u)
  {
    volume = 255u;
  }

  amplitude = (int16_t)(600 + (int16_t)(volume * 10u));
  for (index = 0u; index < sample_count; ++index)
  {
    uint32_t phase = index % period;
    samples[index] = (phase < (period / 2u)) ? amplitude : (int16_t)-amplitude;
  }

  (void)SDL_QueueAudio(backend->audio_device, samples, sample_count * sizeof(*samples));
  record_audio_samples(backend, samples, sample_count);
  SDL_PauseAudioDevice(backend->audio_device, 0);
  free(samples);
}

static int read_le16(const uint8_t *data)
{
  return (int)data[0] | ((int)data[1] << 8);
}

static int read_be16(const uint8_t *data)
{
  return ((int)data[0] << 8) | (int)data[1];
}

static void queue_beep_sequence(SdlBackend *backend, const uint8_t *data, uint32_t length)
{
  uint32_t offset;
  uint32_t tone_count;

  if (!backend || !data)
  {
    return;
  }

  if (backend->audio_device != 0u)
  {
    SDL_ClearQueuedAudio(backend->audio_device);
  }

  tone_count = 0u;
  for (offset = 0u; offset + 5u <= length && tone_count < 16u; offset += 5u)
  {
    uint32_t frequency = (uint32_t)read_le16(&data[offset]);
    uint32_t duration_ms = (uint32_t)read_le16(&data[offset + 2u]);
    uint32_t volume = data[offset + 4u];

    if (frequency != 0u && duration_ms != 0u && volume != 0u)
    {
      queue_square_tone(backend, frequency, duration_ms, volume);
    }
    ++tone_count;
  }
}

static int is_standard_midi_file(const uint8_t *data, uint32_t length, uint16_t *format, uint16_t *tracks)
{
  if (!data || length < 14u)
  {
    return 0;
  }

  if (memcmp(data, "MThd", 4u) != 0)
  {
    return 0;
  }

  if (data[4] != 0u || data[5] != 0u || data[6] != 0u || data[7] != 6u)
  {
    return 0;
  }

  if (format)
  {
    *format = (uint16_t)read_be16(data + 8u);
  }
  if (tracks)
  {
    *tracks = (uint16_t)read_be16(data + 10u);
  }

  return 1;
}

static void process_backend_sound_requests(MpnVM_t *vm, SdlBackend *backend)
{
  MVM_SoundRequest_t request;
  uint32_t type;
  uint8_t *sound_data;

  while (MVM_PollSoundRequest(vm, &request))
  {
    if ((request.flags & SOUND_FLAG_STOP) != 0u)
    {
      stop_backend_midi(backend);
      if (backend && backend->audio_device != 0u)
      {
        SDL_ClearQueuedAudio(backend->audio_device);
      }
      continue;
    }

    if ((request.flags & SOUND_FLAG_STREAM) != 0u)
    {
      fprintf(stderr,
              "vPlayResource stream playback is not implemented yet: handle=%08X length=%u flags=%08X\n",
              request.data,
              request.length,
              request.flags);
      continue;
    }

    if (request.length == 0u)
    {
      continue;
    }

    sound_data = (uint8_t *)malloc((size_t)request.length);
    if (!sound_data)
    {
      continue;
    }

    if (!MVM_ReadGuestMemory(vm, request.data, sound_data, (size_t)request.length))
    {
      fprintf(stderr,
              "vPlayResource guest memory read failed: data=%08X length=%u flags=%08X\n",
              request.data,
              request.length,
              request.flags);
      free(sound_data);
      continue;
    }

    type = request.flags & SOUND_RESOURCE_TYPE_MASK;
    if (type == SOUND_TYPE_BEEP)
    {
      queue_beep_sequence(backend, sound_data, request.length);
    }
    else if (type == SOUND_TYPE_MIDI)
    {
      uint16_t format = 0u;
      uint16_t tracks = 0u;

      if (is_standard_midi_file(sound_data, request.length, &format, &tracks))
      {
        int16_t *pcm = NULL;
        uint32_t sample_count = 0u;

        SDL_BACKEND_LOG_D("vPlayResource MIDI request: data=%08X length=%u format=%u tracks=%u loop=%u\n",
                          request.data,
                          request.length,
                          (uint32_t)format,
                          (uint32_t)tracks,
                          (request.flags & SOUND_FLAG_LOOP) != 0u);

        stop_backend_midi(backend);
        if (backend && backend->audio_device != 0u)
        {
          SDL_ClearQueuedAudio(backend->audio_device);
        }

        if (MidiRenderer_RenderMidi(backend->midi_renderer,
                                    sound_data,
                                    request.length,
                                    (request.flags & SOUND_FLAG_LOOP) != 0u,
                                    &pcm,
                                    &sample_count))
        {
          (void)SDL_QueueAudio(backend->audio_device, pcm, sample_count * sizeof(*pcm));
          record_audio_samples(backend, pcm, sample_count);
          SDL_PauseAudioDevice(backend->audio_device, 0);
          free(pcm);
        }
        else if (!play_backend_midi(backend,
                                    sound_data,
                                    request.length,
                                    (request.flags & SOUND_FLAG_LOOP) != 0u))
        {
          queue_square_tone(backend, 880u, 70u, 80u);
        }
      }
      else
      {
        fprintf(stderr,
                "vPlayResource MIDI data is not SMF: data=%08X length=%u flags=%08X\n",
                request.data,
                request.length,
                request.flags);
        queue_square_tone(backend, 330u, 70u, 48u);
      }
    }
    else if (type == SOUND_TYPE_AMR)
    {
      fprintf(stderr,
              "vPlayResource AMR playback is not implemented yet: data=%08X length=%u flags=%08X\n",
              request.data,
              request.length,
              request.flags);
    }
    else
    {
      fprintf(stderr,
              "vPlayResource unsupported sound type: type=%u data=%08X length=%u flags=%08X\n",
              type,
              request.data,
              request.length,
              request.flags);
    }

    free(sound_data);
  }
}

static void update_backend_raw_button_mask(SdlBackend *backend, uint32_t mask, int is_down)
{
  uint32_t now_ms;
  uint32_t i;

  if (!backend || mask == 0u)
  {
    return;
  }

  now_ms = sdl_platform_get_ticks_ms(backend);
  for (i = 0u; i < SDL_BACKEND_BUTTON_COUNT; ++i)
  {
    if ((mask & MVM_lSdlButtonMasks[i]) == 0u)
    {
      continue;
    }

    if (is_down)
    {
      if ((backend->raw_button_state & MVM_lSdlButtonMasks[i]) == 0u)
      {
        backend->raw_button_state |= MVM_lSdlButtonMasks[i];
        backend->pulse_button_state |= MVM_lSdlButtonMasks[i];
        backend->next_repeat_ms[i] = now_ms + INPUT_REPEAT_DELAY_MS;
      }
    }
    else
    {
      backend->raw_button_state &= ~MVM_lSdlButtonMasks[i];
      backend->next_repeat_ms[i] = 0u;
    }
  }
}

static void update_backend_button_state(SdlBackend *backend, const SDL_KeyboardEvent *key_event)
{
  uint32_t mask;

  if (!backend || !key_event)
  {
    return;
  }

  mask = 0u;
  switch (key_event->keysym.sym)
  {
    case SDLK_UP:
    case SDLK_KP_8:
      mask = MVM_KEY_UP;
      break;

    case SDLK_DOWN:
    case SDLK_KP_2:
      mask = MVM_KEY_DOWN;
      break;

    case SDLK_LEFT:
    case SDLK_KP_4:
      mask = MVM_KEY_LEFT;
      break;

    case SDLK_RIGHT:
    case SDLK_KP_6:
      mask = MVM_KEY_RIGHT;
      break;

    case SDLK_KP_7:
      mask = MVM_KEY_UP | MVM_KEY_LEFT;
      break;

    case SDLK_KP_9:
      mask = MVM_KEY_UP | MVM_KEY_RIGHT;
      break;

    case SDLK_KP_1:
      mask = MVM_KEY_DOWN | MVM_KEY_LEFT;
      break;

    case SDLK_KP_3:
      mask = MVM_KEY_DOWN | MVM_KEY_RIGHT;
      break;

    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
      mask = MVM_KEY_FIRE;
      break;

    case SDLK_BACKSPACE:
    case SDLK_RETURN:
      mask = MVM_KEY_SELECT;
      break;

    case SDLK_SPACE:
    case SDLK_KP_ENTER:
      mask = MVM_KEY_FIRE2;
      break;

    default:
      break;
  }

  if (mask == 0u)
  {
    return;
  }

  update_backend_raw_button_mask(backend, mask, key_event->type == SDL_KEYDOWN);
}

static void update_backend_pointer_state(SdlBackend *backend, const SDL_MouseButtonEvent *button_event)
{
  if (!backend || !button_event)
  {
    return;
  }

  (void)backend;
  (void)button_event;
}

static void refresh_backend_button_pulses(SdlBackend *backend)
{
  uint32_t now_ms;
  uint32_t i;

  if (!backend)
  {
    return;
  }

  now_ms = sdl_platform_get_ticks_ms(backend);
  for (i = 0u; i < SDL_BACKEND_BUTTON_COUNT; ++i)
  {
    if ((backend->raw_button_state & MVM_lSdlButtonMasks[i]) == 0u)
    {
      continue;
    }

    if (backend->next_repeat_ms[i] == 0u)
    {
      continue;
    }

    if (now_ms < backend->next_repeat_ms[i])
    {
      continue;
    }

    backend->pulse_button_state |= MVM_lSdlButtonMasks[i];
    backend->next_repeat_ms[i] = now_ms + INPUT_REPEAT_INTERVAL_MS;
  }
}

SdlBackend *SdlBackend_Create(const MpnDevProfile_t *profile)
{
  SdlBackend *backend;
  uint32_t width;
  uint32_t height;

  backend = (SdlBackend *)calloc(1u, sizeof(*backend));
  if (!backend)
  {
    return NULL;
  }

  backend->last_clear_serial = 0xFFFFFFFFu;
  width = 320u;
  height = 240u;
  if (profile)
  {
    if (profile->screen_width != 0u)
    {
      width = profile->screen_width;
    }

    if (profile->screen_height != 0u)
    {
      height = profile->screen_height;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0)
  {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    free(backend);

    return NULL;
  }

  backend->window = SDL_CreateWindow("Mophun VM",
                                     SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     (int)(width * 2u),
                                     (int)(height * 2u),
                                     SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!backend->window)
  {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SdlBackend_Destroy(backend);

    return NULL;
  }

  backend->renderer = SDL_CreateRenderer(backend->window,
                                         -1,
                                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (!backend->renderer)
  {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SdlBackend_Destroy(backend);

    return NULL;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  SDL_RenderSetLogicalSize(backend->renderer, (int)width, (int)height);

  backend->framebuffer = SDL_CreateTexture(backend->renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           (int)width,
                                           (int)height);
  if (!backend->framebuffer)
  {
    fprintf(stderr, "SDL_CreateTexture framebuffer failed: %s\n", SDL_GetError());
    SdlBackend_Destroy(backend);

    return NULL;
  }

  SDL_SetTextureBlendMode(backend->framebuffer, SDL_BLENDMODE_NONE);
  backend->displaybuffer = SDL_CreateTexture(backend->renderer,
                                             SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_TARGET,
                                             (int)width,
                                             (int)height);
  if (!backend->displaybuffer)
  {
    fprintf(stderr, "SDL_CreateTexture displaybuffer failed: %s\n", SDL_GetError());
    SDL_DestroyTexture(backend->framebuffer);
    backend->framebuffer = NULL;
    SDL_DestroyRenderer(backend->renderer);
    backend->renderer = NULL;
    SDL_DestroyWindow(backend->window);
    backend->window = NULL;
    free(backend);
    return NULL;
  }
  SDL_SetTextureBlendMode(backend->displaybuffer, SDL_BLENDMODE_NONE);

  {
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    SDL_zero(desired);
    desired.freq = 22050;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 1024;

    backend->audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (backend->audio_device != 0u)
    {
      backend->audio_spec = obtained;
      backend->midi_renderer = MidiRenderer_Create(DEFAULT_SOUNDFONT_PATH, (uint32_t)obtained.freq);
      if (!backend->midi_renderer)
      {
        fprintf(stderr, "MIDI renderer init failed: %s\n", DEFAULT_SOUNDFONT_PATH);
      }
    }
    else
    {
      fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    }
  }

  backend->width = width;
  backend->height = height;

  return backend;
}

int SdlBackend_StartRecording(SdlBackend *backend, const char *record_dir)
{
  char path[MAX_PATH];
  uint32_t sample_rate;

  if (!backend || !record_dir || !*record_dir)
  {
    return 0;
  }

  SdlBackend_StopRecording(backend);

  if (!build_record_path(path, sizeof(path), record_dir, "frames.rgba"))
  {
    return 0;
  }
  backend->record_frame_file = fopen(path, "wb");
  if (!backend->record_frame_file)
  {
    return 0;
  }

  if (!build_record_path(path, sizeof(path), record_dir, "audio.wav"))
  {
    SdlBackend_StopRecording(backend);
    return 0;
  }
  backend->record_audio_file = fopen(path, "wb+");
  if (!backend->record_audio_file)
  {
    SdlBackend_StopRecording(backend);
    return 0;
  }

  sample_rate = get_record_audio_sample_rate(backend);
  write_wav_header(backend->record_audio_file, sample_rate, 0u);

  if (build_record_path(path, sizeof(path), record_dir, "recording.txt"))
  {
    backend->record_meta_file = fopen(path, "w");
    if (backend->record_meta_file)
    {
      fprintf(backend->record_meta_file,
              "width=%u\nheight=%u\nfps=%u\naudio_sample_rate=%u\naudio_channels=1\naudio_bits=16\n",
              backend->width,
              backend->height,
              RECORDING_FPS,
              sample_rate);
      fflush(backend->record_meta_file);
    }
  }

  backend->record_start_ms = sdl_platform_get_ticks_ms(backend);
  backend->record_next_frame_ms = 0u;
  backend->record_frame_count = 0u;
  backend->record_audio_sample_count = 0u;
  backend->record_audio_bytes = 0u;

  return 1;
}

void SdlBackend_StopRecording(SdlBackend *backend)
{
  uint32_t sample_rate;
  uint32_t target_audio_bytes;

  if (!backend)
  {
    return;
  }

  sample_rate = get_record_audio_sample_rate(backend);
  target_audio_bytes = 0u;

  if (backend->record_audio_file && backend->record_frame_count != 0u)
  {
    uint32_t video_ms;

    video_ms = (backend->record_frame_count * 1000u + RECORDING_FPS - 1u) / RECORDING_FPS;
    record_audio_pad_to_ms(backend, video_ms);
    target_audio_bytes = (uint32_t)((((uint64_t)sample_rate * video_ms) / 1000u) * sizeof(int16_t));
    if (backend->record_audio_bytes > target_audio_bytes)
    {
      backend->record_audio_bytes = target_audio_bytes;
#ifdef _WIN32
      (void)_chsize(_fileno(backend->record_audio_file), 44L + (long)backend->record_audio_bytes);
#endif
    }
  }

  if (backend->record_audio_file)
  {
    fflush(backend->record_audio_file);
    fseek(backend->record_audio_file, 0L, SEEK_SET);
    write_wav_header(backend->record_audio_file, sample_rate, backend->record_audio_bytes);
    fclose(backend->record_audio_file);
    backend->record_audio_file = NULL;
  }

  if (backend->record_frame_file)
  {
    fclose(backend->record_frame_file);
    backend->record_frame_file = NULL;
  }

  if (backend->record_meta_file)
  {
    fprintf(backend->record_meta_file, "frames=%u\naudio_bytes=%u\n", backend->record_frame_count, backend->record_audio_bytes);
    fclose(backend->record_meta_file);
    backend->record_meta_file = NULL;
  }
}

void SdlBackend_Destroy(SdlBackend *backend)
{
  if (!backend)
  {
    return;
  }

  SdlBackend_StopRecording(backend);

  if (backend->framebuffer)
  {
    SDL_DestroyTexture(backend->framebuffer);
    backend->framebuffer = NULL;
  }
  if (backend->displaybuffer)
  {
    SDL_DestroyTexture(backend->displaybuffer);
    backend->displaybuffer = NULL;
  }

  if (backend->audio_device != 0u)
  {
    SDL_CloseAudioDevice(backend->audio_device);
    backend->audio_device = 0u;
  }

  stop_backend_midi(backend);
  MidiRenderer_Destroy(backend->midi_renderer);
  backend->midi_renderer = NULL;

  if (backend->renderer)
  {
    SDL_DestroyRenderer(backend->renderer);
    backend->renderer = NULL;
  }

  if (backend->window)
  {
    SDL_DestroyWindow(backend->window);
    backend->window = NULL;
  }

  SDL_Quit();
  free(backend);
}

void SdlBackend_AttachVmTiming(MpnVM_t *vm, SdlBackend *backend)
{
  if (!vm || !backend)
  {
    return;
  }

  (void)MVM_SetTickProvider(vm, backend, sdl_platform_get_ticks_ms);
}

void SdlBackend_SetSyntheticButtons(SdlBackend *backend, uint32_t button_mask)
{
  if (!backend)
  {
    return;
  }

  backend->synthetic_button_state = button_mask;
}

void SdlBackend_SyncInputToVm(MpnVM_t *vm, SdlBackend *backend)
{
  if (!vm || !backend)
  {
    return;
  }

  (void)MVM_SetButtonState(vm,
                           backend->raw_button_state |
                           backend->pulse_button_state |
                           backend->synthetic_button_state);
  backend->pulse_button_state = 0u;
}

int SdlBackend_PumpEvents(MpnVM_t *vm, SdlBackend *backend)
{
  SDL_Event event;
  int quit_requested;

  quit_requested = 0;
  while (SDL_PollEvent(&event) != 0)
  {
    if (event.type == SDL_QUIT)
    {
      MVM_RequestExit(vm);
      quit_requested = 1;
      break;
    }
    else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
    {
      update_backend_button_state(backend, &event.key);
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
    {
      update_backend_pointer_state(backend, &event.button);
    }
  }

  refresh_backend_button_pulses(backend);
  SdlBackend_SyncInputToVm(vm, backend);
  process_backend_sound_requests(vm, backend);

  return quit_requested;
}

void SdlBackend_Present(MpnVM_t *vm, SdlBackend *backend)
{
  MVM_RenderFrameInfo_t frame_info;
  MVM_RenderBackend_t render_backend;
  uint32_t color;
  uint32_t first_command;
  int frame_changed;

  if (!backend || !backend->renderer || !backend->framebuffer || !backend->displaybuffer || !vm)
  {
    return;
  }

  process_backend_sound_requests(vm, backend);

  if (!MVM_RenderGetFrameInfo(vm, &frame_info))
  {
    return;
  }

  color = frame_info.clear_color;
  first_command = backend->last_draw_command_count;
  frame_changed = (frame_info.frame_serial != backend->last_frame_serial);

  if (frame_info.clear_serial != backend->last_clear_serial ||
      frame_info.draw_command_count < backend->last_draw_command_count)
  {
    backend->last_clear_serial = frame_info.clear_serial;
    backend->last_draw_command_count = 0u;
    first_command = 0u;

    SDL_SetRenderTarget(backend->renderer, backend->framebuffer);
    SDL_RenderSetClipRect(backend->renderer, NULL);
    set_renderer_guest_color(backend->renderer, color);
    SDL_RenderClear(backend->renderer);
  }

  SDL_SetRenderTarget(backend->renderer, backend->framebuffer);
  render_backend.user = backend;
  render_backend.set_clip = sdl_render_set_clip;
  render_backend.draw_point = sdl_render_draw_point;
  render_backend.draw_line = sdl_render_draw_line;
  render_backend.fill_rect = sdl_render_fill_rect;
  (void)MVM_RenderReplayCommands(vm, &render_backend, first_command);

  SDL_RenderSetClipRect(backend->renderer, NULL);
  if (MVM_RenderGetFrameInfo(vm, &frame_info))
  {
    backend->last_draw_command_count = frame_info.draw_command_count;
  }

  if (frame_info.frame_serial != backend->last_frame_serial)
  {
    backend->last_frame_serial = frame_info.frame_serial;
  }

  if (frame_changed)
  {
    SDL_SetRenderTarget(backend->renderer, backend->displaybuffer);
    SDL_RenderSetClipRect(backend->renderer, NULL);
    SDL_RenderCopy(backend->renderer, backend->framebuffer, NULL, NULL);
    MVM_RenderConsumeCommands(vm);
    backend->last_draw_command_count = 0u;
  }

  SDL_SetRenderTarget(backend->renderer, backend->displaybuffer);
  record_current_frame(backend);

  if (frame_changed)
  {
    SDL_SetRenderTarget(backend->renderer, NULL);
    SDL_RenderSetClipRect(backend->renderer, NULL);
    SDL_SetRenderDrawColor(backend->renderer, 0u, 0u, 0u, 255u);
    SDL_RenderClear(backend->renderer);
    SDL_RenderCopy(backend->renderer, backend->displaybuffer, NULL, NULL);
    SDL_RenderPresent(backend->renderer);
  }
}

uint32_t SdlBackend_GetTicksMs(SdlBackend *backend)
{
  return sdl_platform_get_ticks_ms(backend);
}

void SdlBackend_Delay(uint32_t delay_ms)
{
  SDL_Delay(delay_ms);
}
