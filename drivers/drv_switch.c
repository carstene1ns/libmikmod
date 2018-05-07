/*	MikMod sound library
	(c) 1998-2014 Miodrag Vallat and others - see file AUTHORS for
	complete list.

	This library is free software; you can redistribute it and/or modify
	it under the terms of the GNU Library General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.
*/

/*==============================================================================
==============================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mikmod_internals.h"

#ifdef DRV_SWITCH

#include <switch.h>
#include <string.h>
#include <malloc.h>

#define SAMPLERATE 48000
#define CHANNELCOUNT 2
#define FRAMERATE (1000 / 30)
#define SAMPLECOUNT (SAMPLERATE / FRAMERATE) * 2
#define BYTESPERSAMPLE 4

AudioOutBuffer out_buf;
AudioOutBuffer *audout_released_buf;
u32 data_size = (SAMPLECOUNT * CHANNELCOUNT * BYTESPERSAMPLE);
u32 buffer_size;
u32 released_count;

static volatile int audio_ready = 0;
static volatile int audio_terminate = 0;
static volatile int playing = 0;

static void SWITCH_Update(void) {
	if (!audio_terminate) {
		if (audio_ready) {
			// play
			audoutWaitPlayFinish(&audout_released_buf, &released_count, U64_MAX);
		}

		void *bufptr = audout_released_buf->buffer;

		MUTEX_LOCK(vars);
		if (playing) {
			// fill
			VC_WriteBytes(bufptr, data_size);
		} else {
			// silence
			memset(bufptr, 0, data_size);
		}
		MUTEX_UNLOCK(vars);

		if (audio_ready) {
			audoutAppendAudioOutBuffer(audout_released_buf);
		}
	}
}

static BOOL SWITCH_IsPresent(void) {
	return 1;
}

static int SWITCH_Init(void) {
	Result rc = 0;

	if (VC_Init())
		return 1;

	audio_terminate = 0;
	audio_ready = 0;

	// alignment
	buffer_size = (data_size + 0xfff) & ~0xfff;

	out_buf.buffer = memalign(0x1000, buffer_size);
	if (out_buf.buffer == NULL) {
		printf("Failed to allocate sample data buffers\n");
		return 1;
	}
	out_buf.buffer_size = buffer_size;
	out_buf.data_size = data_size;
	out_buf.next = NULL;
	out_buf.data_offset = 0;

	// clear
	memset(out_buf.buffer, 0, buffer_size);

	audout_released_buf = NULL;
	released_count = 0;

	// init hardware
	rc = audoutInitialize();
	if (R_SUCCEEDED(rc))
		rc = audoutStartAudioOut();

	if (R_FAILED(rc)) {
		_mm_errno = MMERR_OPENING_AUDIO;
		printf("Failed to initialize Switch audio hardware\n");
		return 1;
	}

	// start playback with a bit of silence
	audoutAppendAudioOutBuffer(&out_buf);

	audio_ready = 1;
	return 0;
}

static void SWITCH_Exit(void)
{
	audio_ready = 0;
	audio_terminate = 1;

	VC_Exit();

	// deinit hardware
	audoutStopAudioOut();
	audoutExit();

	// cleanup
	if (out_buf.buffer) {
		free(out_buf.buffer);
		out_buf.buffer = NULL;
	}
}

static int SWITCH_Reset(void)
{
	VC_Exit();
	return VC_Init();
}

static int SWITCH_PlayStart(void)
{
	VC_PlayStart();
	playing = 1;
	return 0;
}

static void SWITCH_PlayStop(void)
{
	playing = 0;
	VC_PlayStop();
}

MIKMODAPI MDRIVER drv_switch = {
	NULL,
	"Switch Audio",
	"Switch Output Driver v1.0 - by carstene1ns",
	0, 255,
	"switch",
	NULL,
	NULL,

	SWITCH_IsPresent,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	SWITCH_Init,
	SWITCH_Exit,
	SWITCH_Reset,

	VC_SetNumVoices,
	SWITCH_PlayStart,
	SWITCH_PlayStop,
	SWITCH_Update,
	NULL,

	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};

#else

MISSING(drv_switch);

#endif
