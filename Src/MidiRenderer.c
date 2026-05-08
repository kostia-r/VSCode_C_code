#include "MidiRenderer.h"

#define TSF_IMPLEMENTATION
#define TML_IMPLEMENTATION
#include "third_party/TinySoundFont/tsf.h"
#include "third_party/TinySoundFont/tml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIDI_RENDER_BLOCK_SAMPLES      (256U)
#define MIDI_RENDER_MAX_SAMPLES        (44100U * 60U)

struct MidiRenderer
{
  tsf *sound_font;
  uint32_t sample_rate;
};

static uint8_t *read_entire_file(const char *path, size_t *size_out)
{
  FILE *file;
  uint8_t *data;
  long size;

  if (!path || !size_out)
  {
    return NULL;
  }

  *size_out = 0u;
  file = fopen(path, "rb");
  if (!file)
  {
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0)
  {
    fclose(file);
    return NULL;
  }

  size = ftell(file);
  if (size <= 0)
  {
    fclose(file);
    return NULL;
  }

  rewind(file);
  data = (uint8_t *)malloc((size_t)size);
  if (!data)
  {
    fclose(file);
    return NULL;
  }

  if (fread(data, 1u, (size_t)size, file) != (size_t)size)
  {
    free(data);
    fclose(file);
    return NULL;
  }

  fclose(file);
  *size_out = (size_t)size;

  return data;
}

static int append_samples(int16_t **dst, uint32_t *count, uint32_t *capacity, const int16_t *src, uint32_t src_count)
{
  uint32_t required;
  uint32_t new_capacity;
  int16_t *grown;

  if (!dst || !count || !capacity || !src)
  {
    return 0;
  }

  if (src_count == 0u)
  {
    return 1;
  }

  required = *count + src_count;
  if (required < *count)
  {
    return 0;
  }

  if (required > *capacity)
  {
    new_capacity = *capacity ? *capacity : 4096u;
    while (new_capacity < required)
    {
      if (new_capacity > (UINT32_MAX / 2u))
      {
        return 0;
      }
      new_capacity *= 2u;
    }

    grown = (int16_t *)realloc(*dst, (size_t)new_capacity * sizeof(**dst));
    if (!grown)
    {
      return 0;
    }

    *dst = grown;
    *capacity = new_capacity;
  }

  memcpy(*dst + *count, src, (size_t)src_count * sizeof(**dst));
  *count = required;

  return 1;
}

static void process_midi_message(tsf *synth, const tml_message *message)
{
  if (!synth || !message)
  {
    return;
  }

  switch (message->type)
  {
    case TML_PROGRAM_CHANGE:
      (void)tsf_channel_set_presetnumber(synth, message->channel, message->program, message->channel == 9);
      (void)tsf_channel_midi_control(synth, message->channel, TML_ALL_SOUND_OFF, 0);
      break;

    case TML_NOTE_ON:
      if (message->velocity != 0)
      {
        (void)tsf_channel_note_on(synth, message->channel, message->key, message->velocity / 127.0f);
      }
      else
      {
        tsf_channel_note_off(synth, message->channel, message->key);
      }
      break;

    case TML_NOTE_OFF:
      tsf_channel_note_off(synth, message->channel, message->key);
      break;

    case TML_PITCH_BEND:
      (void)tsf_channel_set_pitchwheel(synth, message->channel, message->pitch_bend);
      break;

    case TML_CONTROL_CHANGE:
      (void)tsf_channel_midi_control(synth, message->channel, message->control, message->control_value);
      break;

    default:
      break;
  }
}

MidiRenderer *MidiRenderer_Create(const char *sound_font_path, uint32_t sample_rate)
{
  MidiRenderer *renderer;
  uint8_t *sound_font_data;
  size_t sound_font_size;

  if (!sound_font_path || sample_rate == 0u)
  {
    return NULL;
  }

  sound_font_data = read_entire_file(sound_font_path, &sound_font_size);
  if (!sound_font_data)
  {
    return NULL;
  }

  renderer = (MidiRenderer *)calloc(1u, sizeof(*renderer));
  if (!renderer)
  {
    free(sound_font_data);
    return NULL;
  }

  renderer->sound_font = tsf_load_memory(sound_font_data, (int)sound_font_size);
  free(sound_font_data);
  if (!renderer->sound_font)
  {
    free(renderer);
    return NULL;
  }

  renderer->sample_rate = sample_rate;
  tsf_channel_set_bank_preset(renderer->sound_font, 9, 128, 0);
  tsf_set_output(renderer->sound_font, TSF_MONO, (int)sample_rate, 0.0f);
  (void)tsf_set_max_voices(renderer->sound_font, 64);

  return renderer;
}

void MidiRenderer_Destroy(MidiRenderer *renderer)
{
  if (!renderer)
  {
    return;
  }

  if (renderer->sound_font)
  {
    tsf_close(renderer->sound_font);
  }
  free(renderer);
}

int MidiRenderer_RenderMidi(MidiRenderer *renderer,
                            const uint8_t *midi_data,
                            uint32_t midi_size,
                            int loop,
                            int16_t **pcm_out,
                            uint32_t *sample_count_out)
{
  tml_message *messages;
  tml_message *current;
  tsf *synth;
  int16_t block[MIDI_RENDER_BLOCK_SAMPLES];
  int16_t *pcm;
  uint32_t sample_count;
  uint32_t capacity;
  uint32_t total_samples;
  double msecs;

  if (!renderer || !renderer->sound_font || !midi_data || midi_size == 0u || !pcm_out || !sample_count_out)
  {
    return 0;
  }

  *pcm_out = NULL;
  *sample_count_out = 0u;
  messages = tml_load_memory(midi_data, (int)midi_size);
  if (!messages)
  {
    return 0;
  }

  synth = tsf_copy(renderer->sound_font);
  if (!synth)
  {
    tml_free(messages);
    return 0;
  }

  tsf_set_output(synth, TSF_MONO, (int)renderer->sample_rate, 0.0f);
  (void)tsf_set_max_voices(synth, 64);

  pcm = NULL;
  sample_count = 0u;
  capacity = 0u;
  total_samples = 0u;
  msecs = 0.0;
  current = messages;

  while (total_samples < MIDI_RENDER_MAX_SAMPLES)
  {
    uint32_t block_samples = MIDI_RENDER_BLOCK_SAMPLES;

    msecs += block_samples * (1000.0 / (double)renderer->sample_rate);
    while (current && msecs >= current->time)
    {
      process_midi_message(synth, current);
      current = current->next;
    }

    memset(block, 0, sizeof(block));
    tsf_render_short(synth, block, (int)block_samples, 0);
    if (!append_samples(&pcm, &sample_count, &capacity, block, block_samples))
    {
      free(pcm);
      tml_free(messages);
      tsf_close(synth);
      return 0;
    }

    total_samples += block_samples;
    if (!current && tsf_active_voice_count(synth) == 0)
    {
      break;
    }

    if (loop && !current)
    {
      current = messages;
      msecs = 0.0;
      loop = 0;
    }
  }

  tml_free(messages);
  tsf_close(synth);

  *pcm_out = pcm;
  *sample_count_out = sample_count;

  return sample_count != 0u;
}
