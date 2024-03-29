/*
	snd_sdl.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <SDL_audio.h>
#include <SDL_byteorder.h>

#include "cmd.h"
#include "console.h"
#include "qargs.h"
#include "sound.h"
#include "sys.h"

static dma_t the_shm;
static int  snd_inited;

extern int  desired_speed;
extern int  desired_bits;

static void
paint_audio (void *unused, Uint8 * stream, int len)
{
	int streamsamples;
	int sampleposbytes;
	int samplesbytes;

	if (shm) {
		streamsamples = len / (shm->samplebits / 8);
		sampleposbytes = shm->samplepos * (shm->samplebits / 8);
		samplesbytes = shm->samples * (shm->samplebits / 8);

		shm->samplepos += streamsamples;
		while (shm->samplepos >= shm->samples)
			shm->samplepos -= shm->samples;
		S_PaintChannels (soundtime + streamsamples);

		if (shm->samplepos + streamsamples <= shm->samples)
			memcpy (stream, shm->buffer + sampleposbytes, len);
		else {
			memcpy (stream, shm->buffer + sampleposbytes, samplesbytes - sampleposbytes);
			memcpy (stream + samplesbytes - sampleposbytes, shm->buffer, len - (samplesbytes - sampleposbytes));
		}
		soundtime += streamsamples;
	}
}

qboolean
SNDDMA_Init (void)
{
	SDL_AudioSpec desired, obtained;

	snd_inited = 0;

	/* Set up the desired format */
	desired.freq = desired_speed;
	switch (desired_bits) {
		case 8:
			desired.format = AUDIO_U8;
			break;
		case 16:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				desired.format = AUDIO_S16MSB;
			else
				desired.format = AUDIO_S16LSB;
			break;
		default:
			Con_Printf ("Unknown number of audio bits: %d\n", desired_bits);
			return 0;
	}
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = paint_audio;

	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) < 0) {
		Con_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
		return 0;
	}

	/* Make sure we can support the audio format */
	switch (obtained.format) {
		case AUDIO_U8:
			/* Supported */
			break;
		case AUDIO_S16LSB:
		case AUDIO_S16MSB:
			if (((obtained.format == AUDIO_S16LSB) &&
				 (SDL_BYTEORDER == SDL_LIL_ENDIAN)) ||
				((obtained.format == AUDIO_S16MSB) &&
				 (SDL_BYTEORDER == SDL_BIG_ENDIAN))) {
				/* Supported */
				break;
			}
			/* Unsupported, fall through */ ;
		default:
			/* Not supported -- force SDL to do our bidding */
			SDL_CloseAudio ();
			if (SDL_OpenAudio (&desired, NULL) < 0) {
				Con_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
				return 0;
			}
			memcpy (&obtained, &desired, sizeof (desired));
			break;
	}
	SDL_LockAudio();
	SDL_PauseAudio (0);

	/* Fill the audio DMA information block */
	shm = &the_shm;
	shm->splitbuffer = 0;
	shm->samplebits = (obtained.format & 0xFF);
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = obtained.samples * 16;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = calloc(shm->samples * (shm->samplebits / 8), 1);
	if (!shm->buffer)
	{
		Sys_Error ("Failed to allocate buffer for sound!\n");
	}

	snd_inited = 1;
	return 1;
}

int
SNDDMA_GetDMAPos (void)
{
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		SDL_PauseAudio (1);
		SDL_UnlockAudio ();
		SDL_CloseAudio ();
		snd_inited = 0;
		shm = NULL;
	}
}

/*

	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer

*/
void
SNDDMA_Submit (void)
{
	SDL_UnlockAudio();
	SDL_LockAudio();
}
