#ifndef MIDI_RENDERER_H
#define MIDI_RENDERER_H

#include <stdint.h>

typedef struct MidiRenderer MidiRenderer;

MidiRenderer *MidiRenderer_Create(const char *sound_font_path, uint32_t sample_rate);
void MidiRenderer_Destroy(MidiRenderer *renderer);
int MidiRenderer_RenderMidi(MidiRenderer *renderer,
                            const uint8_t *midi_data,
                            uint32_t midi_size,
                            int loop,
                            int16_t **pcm_out,
                            uint32_t *sample_count_out);

#endif
