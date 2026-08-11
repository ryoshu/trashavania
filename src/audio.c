/* audio.c -- stub for now; real pulse/triangle/noise driver lands with the
   audio milestone. Keeping the API live lets game code request sounds. */
#include "audio.h"

void audio_init(void) {}
void audio_frame(void) {}
void audio_sfx(unsigned char id) { (void)id; }
void audio_song(unsigned char id) { (void)id; }
