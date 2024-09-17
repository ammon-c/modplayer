/*
--------------------------------------------------------------------

sss.c

C functions for Simple Sound System, a multi-channel digital
audio playback library for Windows applications.  

--------------------------------------------------------------------

(C) Copyright 1993,1995 Ammon R. Campbell.

I wrote this code for use in my own educational and experimental
programs, but you may also freely use it in yours as long as you
abide by the following terms and conditions:

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.
  * The name(s) of the author(s) and contributors (if any) may not
    be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.  IN OTHER WORDS, USE AT YOUR OWN RISK, NOT OURS.  

--------------------------------------------------------------------
*/

/**************************** INCLUDES ****************************/

#define STRICT
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> 
#include <malloc.h>

#include "sss.h"

/*
** If USE_MM_TIMERS is defined, the code will use Window's
** multimedia timer services; otherwise the regular timer
** services will be used.
*/
#define USE_MM_TIMERS

/**************************** CONSTANTS ***************************/

/*
** MILLISECONDS_PER_TIMER_HIT:  Delay between each timer
** callback.  Under 20ms or so doesn't seem to work well
** Windows.
*/
// #define MILLISECONDS_PER_TIMER_HIT   20
 #define MILLISECONDS_PER_TIMER_HIT     100

/*
** BUFFERS_PER_SECOND:  Number of audio buffers to play
** per second.  Used to calculate the size of the audio
** buffers.
*/
//#define BUFFERS_PER_SECOND            25      /* Very jittery */
//#define BUFFERS_PER_SECOND            10      /* Fairly jittery */
//#define BUFFERS_PER_SECOND            5       /* Marginally clean */
//#define BUFFERS_PER_SECOND            3       /* Infrequent jitter. */
#define BUFFERS_PER_SECOND              2       /* Very clean */

/*
** IDLE:  Value for 'isample' field of channel descriptor to indicate
** that no sample is currently playing on that channel.
*/
#define IDLE            (SSS_MAX_SAMPLES)

/* Play modes for 'playmode' field of song descriptor. */
#define PLAYMODE_STOPPED        0
#define PLAYMODE_PLAYING        1
#define PLAYMODE_PAUSED         2
#define PLAYMODE_REWINDING      3
#define PLAYMODE_FASTFORWARDING 4

/**************************** TYPES *******************************/

/* Struct used to describe one channel. */
typedef struct
{
    UINT    pan_pos;        /* Current stereo pan position of channel. */
    UINT    isample;        /* Index of playing sample (IDLE for none). */
    long    vsize;          /* Virtual size of sample at mixing rate. */
    long    voffset;        /* Current position in sample. */
    signed char *volume;    /* Pointer to volume table for this channel's
                            ** current volume setting. */
} CHANNEL_DESC;

/* Struct used to describe a sample. */
typedef struct
{
    LPSTR   data;           /* 8-bit PCM audio data of sample. */
    UINT    size;           /* Size of sample data in bytes. */
    UINT    loop_start;     /* Position in sample for looping. */
    UINT    loop_size;      /* How much of sample to repeat (0 if non-looping). */
    UINT    smprate;        /* Rate in Hertz at which data was recorded. */
} SAMPLE_DESC;

/* Struct used to describe one pattern for a song. */
typedef struct
{
    UINT            nsteps; /* Number of steps allocated. */
    SSS_STEP_DESC   *steps; /* Alloc'd array of step data. */
} MUSICPATTERN_DESC;

/* Struct used to describe a song. */
typedef struct
{
    /* The data for the song. */
    UINT            npatterns;      /* Number of patterns allocated. */
                                    /* Zero if no song is loaded. */
    MUSICPATTERN_DESC *patterns;    /* Alloc'd array of pattern data. */
    UINT            norder;         /* Number of entries in pattern order list. */
    UINT            *order;         /* Alloc'd array of pattern play order. */
                                    /* Each specifies an index of a pattern. */
    UINT            nsamples;       /* Number of sample handles. */
    UINT            *samples;       /* Alloc'd array of sample handles. */
    UINT            pan_pos[SSS_MUSIC_CHANNELS];
                                    /* Initial pan positons for each channel. */

    /* Running status for the song. */
    UINT            playmode;       /* Mode (play/pause/foward/rewind). */
    UINT            iorder;         /* Current place in order list. */
    UINT            ipattern;       /* Current pattern. */
    UINT            istep;          /* Current step in pattern. */
    DWORD           song_pos;       /* Current position in song (samples). */
    DWORD           step_delay;     /* Delay between each step (samples). */
                                    /* Determines tempo. */
} MUSICSONG_DESC;

/**************************** DATA ********************************/

/* initialized:  Non-zero if library has been initialized. */
static BOOL initialized = 0;

/* mixrate:  Mixing (playback) rate of audio device in Hertz. */
static UINT mixrate = 0;

/* is_stereo:  Flag; nonzero if output device supports stereo. */
static UINT is_stereo = 0;

/* chan:  Array of audio channel descriptors. */
static CHANNEL_DESC chan[SSS_MAX_CHANNELS];

/* samples:  Array of sample descriptors. */
static SAMPLE_DESC samples[SSS_MAX_SAMPLES];

/* hwaveout:  Handle to wave output device from waveOutOpen() */
static HWAVEOUT hwaveout;

/* bfr_size:  Size of each wave output buffer in bytes. */
static UINT bfr_size = 0;

/* hbuffers:  Global memory handles of our alloc'd buffers for
** audio data in WAVEHDRs. */
static HGLOBAL hbuffers[2] = { NULL, NULL };

/* buffers:  GlobalLock'd pointers for the hbuffers[] handles. */
static LPSTR buffers[2] = { NULL, NULL };

/* wavehdrs:  Array of WAVEHDR structs for calling waveOutWrite() */
static WAVEHDR wavehdrs[2];

/* bfr_toggle:  Flag that flips back and forth between two sets
** of output buffers as they are played (0 or 1 depending). */
static UINT bfr_toggle = 0;

#ifdef USE_MM_TIMERS
/* timer_id:  Multimedia timer ID, as returned by timeSetEvent() */
static MMRESULT timer_id = 0xFFFF;
#else
/* timer_id:  Windows timer ID, as returned by SetTimer() */
static UINT timer_id = 0;
#endif /* USE_MM_TIMERS */

/* profiling variables. */
static long prof_count_polls = 0;
static long prof_count_recursive_polls = 0;
static long prof_count_writes = 0;
static long prof_count_idle_polls = 0;

/* Current song. */
static MUSICSONG_DESC song;

/* Number of samples mixed so far. */
/* Used for timing music. */
static DWORD song_counter = 0L;

/*
** Tables for translating sample volumes.
** The volume of a sample ranges from
** 0 to 15, and is the primary index into
** the table.  The secondary index is the
** signed sample byte to be translated.
*/
static signed char volume_tables[SSS_MAX_VOLUME][256];

/*
** Current volume setting for music playback.
*/
static UINT music_volume = SSS_MAX_VOLUME * 3 / 4;

/************************* LOCAL FUNCTIONS ************************/

/*
** music_stop:
** Stops playback of music.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
static void
music_stop(void)
{
    UINT    u;

    /* Stop all channels that were used for music. */
    for (u = 0; u < SSS_MUSIC_CHANNELS; u++)
    {
        sss_channel_stop(SSS_MUSIC_FIRST + u);
    }

    /* Stop playing the song. */
    song.playmode = PLAYMODE_STOPPED;
    song_counter = 0L;
    song.song_pos = 0L;
}

/*
** music_play:
** Begins playback of music.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
static void
music_play(void)
{
    UINT u;

    /* See if a song is loaded. */
    if (song.npatterns == 0)
        return;

    /* If music was paused, rewinding, or fastforwarding, then go
    ** back to normal playback mode. */
    if (song.playmode == PLAYMODE_PAUSED ||
        song.playmode == PLAYMODE_REWINDING ||
        song.playmode == PLAYMODE_FASTFORWARDING)
    {
        song.playmode = PLAYMODE_PLAYING;
        return;
    }

    /* See if music is already playing. */
    if (song.playmode == PLAYMODE_PLAYING)
    {
        /* Already playing. */
        return;
    }

    /* If music is already playing, stop it. */
    music_stop();

    /* Set initial pan positions for each music channel. */
    for (u = 0; u < SSS_MUSIC_CHANNELS; u++)
    {
        sss_channel_pan_set(SSS_MUSIC_FIRST + u, song.pan_pos[u]);
    }

    /* Start the music. */
    song_counter = 0L;
    song.song_pos = 0L;
    song.step_delay = ((long)mixrate * (1 + 7)) / 67L;
    song.iorder = 0;
    song.ipattern = 0;
    song.istep = 0;
    song.playmode = PLAYMODE_PLAYING;
}

/*
** build_volume_tables:
** Initializes the contents of the volume_tables[]
** arrays.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
static void
build_volume_tables(void)
{
    int             volume;
    int             pos;
    int             ival;
    signed char     cval;

    for (volume = 0; volume < SSS_MAX_VOLUME; volume++)
    {
        for (pos = 0; pos < 256; pos++)
        {
            ival = pos - 127;
            ival = ival * volume / 15;
            cval = (signed char)ival;
            volume_tables[volume][pos] = cval;
        }
    }
}

/*
** music_poll:
** Called periodically by mix().  Determines when to play
** samples for the currently playing music.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      songp   Current song position.
**
** Returns:
**      NONE
*/
static void
music_poll(DWORD songp)
{
    UINT            ichannel;
    SSS_STEP_DESC   *step;
    UINT            dobreak;

    /* Is a song playing? */
    if (song.patterns == NULL ||
            song.playmode == PLAYMODE_PAUSED ||
            song.playmode == PLAYMODE_STOPPED)
    {
        /* No song playing, or song is paused. */
        return;
    }

    /* Play any steps whose time has come. */
    while (song.song_pos < songp && song.playmode != PLAYMODE_STOPPED)
    {
        /* Get pattern index for this pattern in play order. */
        song.ipattern = song.order[song.iorder];

        /* Process notes in this step of the pattern. */
        step = &song.patterns[song.ipattern].steps[song.istep];
        dobreak = 0;
        for (ichannel = 0; ichannel < SSS_MUSIC_CHANNELS; ichannel++)
        {
            if (dobreak)
                break;

            /* Play a note on this channel? */
            if (step->note_pitch[ichannel] != 0)
            {
                sss_sample_play(SSS_MUSIC_FIRST + ichannel,
                        song.samples[step->note_sample[ichannel]],
                        step->note_pitch[ichannel]);
                sss_channel_volume(SSS_MUSIC_FIRST + ichannel,
                        music_volume);
            }

            /* Have any effect on this channel? */
            switch(step->note_effect[ichannel])
            {
                case SSS_EFFECT_PATTERN_BREAK:
                    song.istep = 999;
                    dobreak = 1;
                    break;

                case SSS_EFFECT_JUMP:
                    song.istep = 0;
                    song.iorder = step->note_eparam[ichannel];
                    dobreak = 1;
                    continue;
                    break;

                case SSS_EFFECT_SET_TEMPO:
                    if (step->note_eparam[ichannel] != 0)
                        song.step_delay = ((long)mixrate * (1 + (long)step->note_eparam[ichannel])) / 65L;
                    break;

                case SSS_EFFECT_SET_VOLUME:
                    sss_channel_volume(SSS_MUSIC_FIRST + ichannel,
                            step->note_eparam[ichannel] * music_volume / 63);
                    break;

                case SSS_EFFECT_NONE:
                default:
                        ; /* Do nothing. */
            }
        }

        /* Update current position in song. */
        song.song_pos += song.step_delay;

        /* Step to next step. */
        song.istep++;

        /* Step to next pattern if ready. */
        if (song.istep >= song.patterns[song.ipattern].nsteps)
        {
            song.iorder++;
            song.istep = 0;
        }

        /* See if song is done yet. */
        if (song.iorder >= song.norder)
        {
            /* Song is finished. */
            song.playmode = PLAYMODE_STOPPED;
            song.song_pos = 0L;
            song_counter = 0L;
            song.istep = 0;
            song.iorder = 0;
            song.ipattern = 0;
        }
    }
}

/*
** mix:
** Mixes a buffer full of audio data based on the samples
** currently playing on the audio channels.  Updates the
** state of various variable in this module to reflect the
** time advancement of the mix.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
static void
mix(void)
{
    UINT    u;              /* Loop index. */
    UINT    step;           /* Number of bytes per sample in this buffer. */
    UINT    ch;             /* Channel loop index. */
    UINT    offset;         /* Offset into sample data. */
    int     mixval_l;       /* Intermediate value for mixing, left. */
    int     mixval_r;       /* Intermediate value for mixing, right. */
    SAMPLE_DESC *psample;   /* Pointer to current sample. */
    int     ival;           /* Temporary signed integer for mixing. */

    /* Determine how to step through the audio buffer. */
    step = 1;
    if (is_stereo)
    {
        step *= 2;
    }

    /* Step through each sample in the audio buffer. */
    for (u = 0; u < bfr_size; u += step)
    {
        /* Poll for music a few times per buffer. */
        if (u == 0 || (u >> 1) % ((mixrate / 64) >> is_stereo) == 0)
        {
            music_poll(song_counter + (u / step));
        }

        /* Assume nil volume. */
        mixval_l = 0;
        mixval_r = 0;

        /* Handle each channel. */
        for (ch = 0; ch < SSS_MAX_CHANNELS; ch++)
        {
            /* Is this channel playing something? */
            if (chan[ch].isample == IDLE)
            {
                /* This channel is not playing. */
                continue;
            }

            /* Get a pointer to the sample on this channel. */
            psample = &samples[chan[ch].isample];

            /* Calculate actual offset into sample data. */
            offset = (UINT)((chan[ch].voffset *
                            (long)psample->size) /
                            chan[ch].vsize);

            /* End of this sample yet? */
            if (offset >= psample->size ||
                (offset >= (psample->loop_start +
                    psample->loop_size) &&
                    (psample->loop_size > 0)))
            {
                /* Looping sample or not? */
                if (psample->loop_size > 2)
                {
                    /* End of looping sample; repeat it. */
                    chan[ch].voffset = psample->loop_start;
                    continue;
                }
                else
                {
                    /* End of sample; stop playing it. */
                    chan[ch].isample = IDLE;
                    chan[ch].voffset = 0;
                    chan[ch].vsize = 0;
                    continue;
                }
            }

            /* Merge byte of sample data into mix. */
            ival = (int)psample->data[offset] + 128;
            ival = chan[ch].volume[ival];
            if (is_stereo)
            {
                mixval_l += volume_tables[SSS_MAX_VOLUME - 1 - chan[ch].pan_pos][ival + 128];
                mixval_r += volume_tables[chan[ch].pan_pos][ival + 128];
            }
            else
            {
                mixval_l += ival;
            }

            /* Step to next relative offset. */
            chan[ch].voffset++;
        }

        /* Scale mixed value back down and uncenter. */
        mixval_l >>= 2;
        mixval_l += 127;
        mixval_r >>= 2;
        mixval_r += 127;

        /* Put mixed value into buffer. */
        buffers[bfr_toggle][u] = (unsigned char)mixval_l;
        if (is_stereo)
        {
            buffers[bfr_toggle][u + 1] = (unsigned char)mixval_r;
        }
    }

    /* Update song time counter. */
    if (song.playmode == PLAYMODE_PLAYING)
    {
        /* Normal play mode. */
        song_counter += (DWORD)bfr_size / step;
    }
    else if (song.playmode == PLAYMODE_FASTFORWARDING)
    {
        /* FFWD:  Play 4x normal speed */
        song_counter += (DWORD)bfr_size * 4 / step;
    }
    else if (song.playmode == PLAYMODE_REWINDING)
    {
        /* REWIND:  Back up 4x normal speed */
        if (song_counter > (DWORD)bfr_size * 4 / step)
        {
            /* Also back up the song_counter, so we
            ** can hear as we are rewinding. */
            song_counter -= (DWORD)bfr_size * 4 / step;
            if (song_counter > bfr_size)
                song.song_pos = song_counter - bfr_size;
            else
                song.song_pos = 0;
        }
        else
        {
            /* Rewound to beginning of song. */
            music_stop();
        }
    }
}

/*
** sss_poll:
** Polling function to drive mixing.  This is called frequently.
** It determines if it is time to toggle and mix the other set
** of buffers yet.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
static void
sss_poll(void)
{
    static UINT busy = 0;   /* Busy flag, to prevent recursive entry. */

    prof_count_polls++;

    /* Prevent recursive entry. */
    if (busy)
    {
        /* Already in this routine. */
        prof_count_recursive_polls++;
        return;
    }

    /* Is the current buffer finished playing? */
    if (!(wavehdrs[bfr_toggle].dwFlags & WHDR_DONE))
    {
        /* Still waiting for buffer. */
        prof_count_idle_polls++;
        return;
    }

    /* Set busy flag. */
    busy = 1;

    /*
    ** A buffer is done being played.
    ** Fill it up with mix data again
    ** and queue it to be played.
    */

    /* Turn off the 'done' flag. */
    wavehdrs[bfr_toggle].dwFlags &= ~WHDR_DONE;

    /* Mix the next bufferfull of audio data. */
    mix();

#if 0
    /* Unprepare the next header. */
    waveOutUnprepareHeader(hwaveout,
                    (LPWAVEHDR)&wavehdrs[bfr_toggle],
                    sizeof(WAVEHDR));

    /* Prepare the next header. */
    waveOutPrepareHeader(hwaveout,
                    (LPWAVEHDR)&wavehdrs[bfr_toggle],
                    sizeof(WAVEHDR));
#endif

    /* Queue the next buffer. */
    waveOutWrite(hwaveout, &wavehdrs[bfr_toggle], sizeof(WAVEHDR));
    prof_count_writes++;

    /* Flip the toggle, so we work on the other buffer. */
    bfr_toggle = (bfr_toggle + 1) & 1;

    /* Reset busy flag. */
    busy = 0;
}

#ifdef USE_MM_TIMERS
/*
** sss_mmtimer_callback:
** Function called periodically by the timer activated by
** timeSetEvent() in sss_init().
**
** Parameters:
**      See windows docs.
**
** Returns:
**      NONE
*/
void PASCAL
sss_mmtimer_callback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dwl, DWORD_PTR dw2)
{
    (void)wTimerID;
    (void)msg;
    (void)dwUser;
    (void)dwl;
    (void)dw2;

    sss_poll();
}
#else
/*
** sss_wintimer_callback:
** Function called periodically by the timer activated by
** SetTimer() in sss_init().
**
** Parameters:
**      See windows docs.
**
** Returns:
**      NONE
*/
void CALLBACK /* __declspec(dllexport) */
sss_wintimer_callback(HWND hwnd, UINT msg, UINT idtimer, DWORD dwtime)
{
    (void)hwnd;
    (void)msg;
    (void)idtimer;
    (void)dwtime;

    sss_poll();
}
#endif /* USE_MM_TIMERS */

/**************************** FUNCTIONS ***************************/

/*
** sss_init:
** Performs one-time initialization of the sound library.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      hinst   Instance handle of calling application.
**
** Returns:
**      Value   Meaning
**      -----   -------
**      any     See SSSERR_ constants in sss.h
*/
UINT
sss_init(HINSTANCE hinst)
{
    (void)hinst;

    UINT            u;      /* Loop index. */
    WAVEOUTCAPS     wcaps;  /* Capabilities of wave output device. */
    WAVEFORMATEX    f;
    LPWAVEFORMATEX  wfmt;   /* Audio data format we will use. */

    /* Check if library already initialized. */
    if (initialized)
    {
        /* Already initialized. */
        return SSSERR_ALREADY_INITED;
    }

    /* Mark samples list all empty. */
    for (u = 0; u < SSS_MAX_SAMPLES; u++)
    {
        samples[u].data = NULL;
        samples[u].size = 0;
        samples[u].smprate = 0;
    }

    /* Reset all channels. */
    for (u = 0; u < SSS_MAX_CHANNELS; u++)
    {
        chan[u].pan_pos = SSS_PAN_CENTER;
        chan[u].isample = IDLE;
        chan[u].voffset = 0;
        chan[u].vsize = 0;
        chan[u].volume = &volume_tables[SSS_MAX_VOLUME - 1][0];
    }

    /* Mark song data as unused. */
    memset(&song, 0, sizeof(song));

    /* Build volume tables. */
    build_volume_tables();

    /* Get capabilities of wave output device. */
    memset(&wcaps, 0, sizeof(wcaps));
    if (waveOutGetDevCaps(0, &wcaps, sizeof(wcaps)))
    {
        /* Couldn't get devcaps for audio device. */
        return SSSERR_OPEN_CAPS;
    }

    /* Decide what format to use. */
    memset(&f, 0, sizeof(f));
    wfmt = &f;
    wfmt->wBitsPerSample = 8;
    wfmt->wFormatTag = WAVE_FORMAT_PCM;
    if (wcaps.dwFormats & WAVE_FORMAT_4S08)
    {
        /* 44.1KHz stereo 8-bit */
        wfmt->nChannels = 2;    /* Stereo */
        wfmt->nSamplesPerSec = 44100L;
        wfmt->nAvgBytesPerSec = 88200L;
        wfmt->nBlockAlign = 2;
    }
    else if (wcaps.dwFormats & WAVE_FORMAT_2S08)
    {
        /* 22.05KHz stereo 8-bit */
        wfmt->nChannels = 2;    /* Stereo */
        wfmt->nSamplesPerSec = 22050;
        wfmt->nAvgBytesPerSec = 44100;
        wfmt->nBlockAlign = 2;
    }
    else if (wcaps.dwFormats & WAVE_FORMAT_1S08)
    {
        /* 11.025KHz stereo 8-bit */
        wfmt->nChannels = 2;    /* Stereo */
        wfmt->nSamplesPerSec = 11025;
        wfmt->nAvgBytesPerSec = 22050;
        wfmt->nBlockAlign = 2;
    }
    else if (wcaps.dwFormats & WAVE_FORMAT_4M08)
    {
        /* 44.1KHz mono 8-bit */
        wfmt->nChannels = 1;    /* Mono */
        wfmt->nSamplesPerSec = 44100;
        wfmt->nAvgBytesPerSec = 44100;
        wfmt->nBlockAlign = 1;
    }
    else if (wcaps.dwFormats & WAVE_FORMAT_2M08)
    {
        /* 22.05KHz mono 8-bit */
        wfmt->nChannels = 1;    /* Mono */
        wfmt->nSamplesPerSec = 22050;
        wfmt->nAvgBytesPerSec = 22050;
        wfmt->nBlockAlign = 1;
    }
    else if (wcaps.dwFormats & WAVE_FORMAT_1M08)
    {
        /* 11.025KHz mono 8-bit */
        wfmt->nChannels = 1;    /* Mono */
        wfmt->nSamplesPerSec = 11025;
        wfmt->nAvgBytesPerSec = 11025;
        wfmt->nBlockAlign = 1;
    }
    else
    {
        /* No 8-bit audio formats supported! */
        return SSSERR_OPEN_FORMAT;
    }

    /* Set misc. variables. */
    mixrate = (UINT)wfmt->nSamplesPerSec;
    is_stereo = 0;
    if (wfmt->nChannels > 1)
        is_stereo = 1;
    bfr_size = (UINT)(wfmt->nAvgBytesPerSec / BUFFERS_PER_SECOND);
    bfr_size &= ~0x3;       /* DWORD boundary. */

    /* Open the audio output device. */
    /* NOTE:  The docs say WAVEFORMAT should be passed to
    ** waveOutOpen(), but pointer actually must point to
    ** PCMWAVEFORMAT instead.  Wasted a bunch of time
    ** finding this out. */
    u = waveOutOpen((LPHWAVEOUT)&hwaveout, (UINT)WAVE_MAPPER,
                    wfmt, 0, 0, 0);
    if (u)
    {
#ifdef DBG
        switch(u)
        {
            case MMSYSERR_BADDEVICEID:
                OutputDebugString("MMSYSERR_BADDEVICEID\n");
                break;
            case MMSYSERR_ALLOCATED:
                OutputDebugString("MMSYSERR_ALLOCATED\n");
                break;
            case MMSYSERR_NOMEM:
                OutputDebugString("MMSYSERR_NOMEM\n");
                break;
            case MMSYSERR_INVALPARAM:
                OutputDebugString("MMSYSERR_INVALPARAM\n");
                break;
            case WAVERR_BADFORMAT:
                OutputDebugString("WAVERR_BADFORMAT\n");
                break;
            case WAVERR_SYNC:
                OutputDebugString("WAVERR_SYNC\n");
                break;
            default:
                OutputDebugString("Unknown error\n");
        }
#endif /* DBG */

        /* Couldn't open the audio device. */
        return SSSERR_OPEN_DEVICE;
    }

    /* Allocate buffers for WAVEHDRs. */
    for (u = 0; u < 2; u++)
    {
        hbuffers[u] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE |
                                  GMEM_ZEROINIT,
                                  (DWORD)bfr_size);
        if (hbuffers[u] == NULL)
        {
            /* Out of memory! */
            waveOutClose(hwaveout);
            hwaveout = NULL;
            return SSSERR_NO_MEMORY;
        }
        buffers[u] = GlobalLock(hbuffers[u]);
    }

    /* Set up WAVEHDRs. */
    for (u = 0; u < 2; u++)
    {
        /* Set up one WAVEHDR. */
        memset(&wavehdrs[u], 0, sizeof(WAVEHDR));
        wavehdrs[u].lpData = buffers[u];
        wavehdrs[u].dwBufferLength = (DWORD)bfr_size;
        wavehdrs[u].dwBytesRecorded = (DWORD)bfr_size;

        /* Prepare it. */
        waveOutPrepareHeader(hwaveout,
                        (LPWAVEHDR)&wavehdrs[u],
                        sizeof(WAVEHDR));
    }

    /* Start the audio running by setting the WAVEHDR 'done' flags. */
    bfr_toggle = 0;
    wavehdrs[0].dwFlags |= WHDR_DONE;
    wavehdrs[1].dwFlags |= WHDR_DONE;

    /* Start a timer. */
#ifdef USE_MM_TIMERS
    timeBeginPeriod(5);
    timer_id = timeSetEvent(
                    MILLISECONDS_PER_TIMER_HIT,
                    5,
                    sss_mmtimer_callback,
                    0,
                    TIME_PERIODIC);
#else
    timer_id = SetTimer(NULL,
                    1,      /* Our timer ID */
                    MILLISECONDS_PER_TIMER_HIT,
                    sss_wintimer_callback);
    if (timer_id == 0)
    {
        /*
        ** Couldn't get a timer!
        ** Clean up and bail.
        */

        /* Stop anything that's still playing. */
        waveOutReset(hwaveout);

        /* Unprepare the wave headers. */
        for (u = 0; u < 2; u++)
        {
            waveOutUnprepareHeader(hwaveout,
                            (LPWAVEHDR)&wavehdrs[u],
                            sizeof(WAVEHDR));
        }

        /* Close the audio device. */
        waveOutClose(hwaveout);

        /* Discard buffers that were used for WAVEHDRs. */
        for (u = 0; u < 2; u++)
        {
            if (hbuffers[u] != NULL)
            {
                    GlobalUnlock(hbuffers[u]);
                    GlobalFree(hbuffers[u]);
            }
            hbuffers[u] = NULL;
            buffers[u] = NULL;
        }

        /* Reset variables. */
        mixrate = 0;
        hwaveout = NULL;

        return SSSERR_NO_TIMER;
    }
#endif /* USE_MM_TIMERS */

    /* Mark library as initialized. */
    initialized = 1;

    /* Success! */
    return SSSERR_OK;
}

/*
** sss_deinit:
** Performs one-time shutdown of the sound library.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
void
sss_deinit(void)
{
    UINT    u;

    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return;
    }

    /* Discard music. */
    music_stop();
    sss_music_flush();

    /* Kill the timer. */
#ifdef USE_MM_TIMERS
    timeKillEvent(timer_id);
    timeBeginPeriod(5);
#else
    KillTimer(NULL, timer_id);
#endif /* USE_MM_TIMERS */

    /* Reset all channels. */
    for (u = 0; u < SSS_MAX_CHANNELS; u++)
    {
        chan[u].pan_pos = SSS_PAN_CENTER;
        chan[u].isample = IDLE;
        chan[u].voffset = 0;
        chan[u].vsize = 0;
    }

    /* Stop anything that's still playing. */
    waveOutReset(hwaveout);

    /* Unprepare the wave headers. */
    for (u = 0; u < 2; u++)
    {
        waveOutUnprepareHeader(hwaveout,
                        (LPWAVEHDR)&wavehdrs[u],
                        sizeof(WAVEHDR));
    }

    /* Close the audio device. */
    waveOutClose(hwaveout);

    /* Discard buffers that were used for WAVEHDRs. */
    for (u = 0; u < 2; u++)
    {
        if (hbuffers[u] != NULL)
        {
            GlobalUnlock(hbuffers[u]);
            GlobalFree(hbuffers[u]);
        }
        hbuffers[u] = NULL;
        buffers[u] = NULL;
    }

    /* Reset variables. */
    mixrate = 0;
    hwaveout = NULL;

    /* Discard samples from memory. */
    for (u = 0; u < SSS_MAX_SAMPLES; u++)
    {
        /* Does this sample have allocated memory? */
        if (samples[u].data != NULL)
        {
            free(samples[u].data);
        }

        /* Mark sample as unused. */
        samples[u].data = NULL;
        samples[u].size = 0;
        samples[u].smprate = 0;
    }

    /* Mark library as uninitialized. */
    initialized = 0;
}

/*
** sss_get_mixrate:
** Retrieve the mixing (output) rate of the
** audio device in Hertz.
**
** Parameters:
**      NONE
**
** Returns:
**      Value   Meaning
**      -----   -------
**      any     Mixing rate in Hertz.
*/
UINT
sss_get_mixrate(void)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return 0;
    }

    return mixrate;
}

/*
** sss_get_channel_count:
** Retrieves the number of audio channels
** available for playing samples.
**
** Parameters:
**      NONE
**
** Returns:
**      Value   Meaning
**      -----   -------
**      any     Number of audio channels.
*/
UINT
sss_get_channel_count(void)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return 0;
    }

    /* Caller gets # of channels not used for music. */
    return SSS_MAX_CHANNELS - SSS_MUSIC_CHANNELS;
}

/*
** sss_channel_pan_set:
** Sets the pan position of an audio channel.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      pan     Pan position, between SSS_PAN_LEFT and
**              SSS_PAN_RIGHT, inclusive, or SSS_PAN_CENTER.
**
** Returns:
**      NONE
*/
void
sss_channel_pan_set(UINT channel, UINT pan)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return;
    }

    /* Check channel number. */
    if (channel >= SSS_MAX_CHANNELS)
    {
        return;
    }

    /* Check pan position. */
    if (pan < SSS_PAN_LEFT || pan > SSS_PAN_RIGHT)
    {
        return;
    }

    /* Save new pan position. */
    chan[channel].pan_pos = pan;
}

/*
** sss_channel_pan_get:
** Retrieves the current pan position of an audio
** channel.
**
** Parameters:
**      NONE
**
** Returns:
**      Value   Meaning
**      -----   -------
**      any     Relative pan position of audio
**              channel, between SSS_PAN_LEFT
**              and SSS_PAN_RIGHT, inclusive.
*/
UINT
sss_channel_pan_get(UINT channel)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return SSS_PAN_CENTER;
    }

    /* Check channel number. */
    if (channel >= SSS_MAX_CHANNELS)
    {
        return SSS_PAN_CENTER;
    }

    /* Retrieve current pan position for caller. */
    return chan[channel].pan_pos;
}

/*
** sss_channel_is_busy:
** Determines if a particular channel is busy
** playing a sample or not.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      channel Channel number of channel to interrogate.
**
** Returns:
**      Value   Meaning
**      -----   -------
**      1       Channel is busy.
**      0       Channel is not busy.
*/
UINT
sss_channel_is_busy(UINT channel)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return 0;
    }

    /* Check channel number. */
    if (channel >= SSS_MAX_CHANNELS)
    {
        return 0;
    }

    if (chan[channel].isample != IDLE)
        return 1;

    return 0;
}

/*
** sss_channel_stop:
** Stops any sample that is playing on a particular
** channel.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      ch      Channel number to cease.
**
** Returns:
**      NONE
*/
void
sss_channel_stop(UINT channel)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return;
    }

    /* Check channel number. */
    if (channel >= SSS_MAX_CHANNELS)
    {
        return;
    }

    /* Reset sample for this channel to idle state. */
    chan[channel].isample = IDLE;
    chan[channel].voffset = 0;
    chan[channel].vsize = 0;
}

/*
** sss_channel_volume:
** Sets the relative volume level of a particular
** audio channel.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      channel Channel to modify.
**      v       New volume level from 0..SSS_MAX_VOLUME-1.
**
** Returns:
**      NONE
*/
void
sss_channel_volume(UINT channel, UINT v)
{
    if (!initialized)
            return;

    if (channel >= SSS_MAX_CHANNELS)
            return;

    if (v >= SSS_MAX_VOLUME)
            v = SSS_MAX_VOLUME - 1;
    if (v >= 0xFFFE)
            v = 0;

    chan[channel].volume = &volume_tables[v][0];
}

/*
** sss_sample_add:
** Adds a sample to the list of samples that may be played.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      data    Pointer to 8-bit PCM sample data.
**      size    Size of data in bytes.
**      loopbeg For a looping sample, the offset into the
**              sample data to start playing each time the
**              sample loops.
**      loopsiz For a looping sample, how much sample data
**              to repeat, or zero if not a looping sample.
**      smprate Rate at which sample was recorded in Hertz.
**      center  Flag, specifies whether samples need to be
**              centered or not:
**                      0 = Pre-centered sample (i.e. .SAM)
**                      1 = Non-cnetered sample (i.e. .WAV)
**
** Returns:
**      Value           Meaning
**      -----           -------
**      SSSERR_...      See SSSERR constants in sss.h
**      other           Handle of new sample, a value
**                      from zero to SSS_MAX_SAMPLES-1.
*/
UINT
sss_sample_add(LPSTR data, UINT size,
        UINT loopbeg, UINT loopsiz, UINT smprate, UINT center)
{
    UINT    u;
    UINT    v;

    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return SSSERR_NOT_INITED;
    }

    /* Find an unused sample descriptor. */
    u = 0;
    while (u < SSS_MAX_SAMPLES)
    {
        if (samples[u].data == NULL)
            break;
        u++;
    }
    if (u >= SSS_MAX_SAMPLES)
    {
        /* All entries in samples list already used up. */
        return SSSERR_NO_HANDLES;
    }

    /* Allocate memory for sample data. */
    samples[u].data = malloc(size);
    if (samples[u].data == NULL)
    {
        /* Not enough memory. */
        return SSSERR_NO_MEMORY;
    }

    /* Set up sample descriptor. */
    memcpy(samples[u].data, data, size);
    if (center)
    {
        for (v = 0; v < size; v++)
            samples[u].data[v] = samples[u].data[v] - 128;
    }
    samples[u].size = size;
    samples[u].smprate = smprate;
    samples[u].loop_start = loopbeg;
    samples[u].loop_size = loopsiz;

    /* Caller gets sample list index (sample 'handle'). */
    return u;
}

/*
** sss_sample_delete:
** Deletes a sample that was previously added
** to the samples list by sss_sample_add().
**
** Parameters:
**      Name    Description
**      ----    -----------
**      hsmp    Handle of sample to delete.
**
** Returns:
**      NONE
*/
void
sss_sample_delete(UINT hsmp)
{
    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return;
    }

    /* Check sample handle. */
    if (hsmp >= SSS_MAX_SAMPLES)
    {
        /* Handle out of range. */
        return;
    }

    /* Is sample used? */
    if (samples[hsmp].data == NULL)
    {
        /* This sample not used. */
        return;
    }

    /* Free up the specified sample. */
    free(samples[hsmp].data);
    samples[hsmp].data = NULL;
    samples[hsmp].size = 0;
    samples[hsmp].smprate = 0;
}

/*
** sss_sample_play:
** Begins playing a sample from the samples list.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      ch      Channel number to play sample on.
**      hsmp    Handle of sample to be played.
**      pitch   Sample rate to adjust sample to.
**              By making this larger or smaller
**              than the rate at which the sample
**              was actually recorded, the pitch
**              of the sample playback is changed.
**
** Returns:
**      NONE
*/
void
sss_sample_play(UINT channel, UINT hsmp, UINT pitch)
{
    DWORD   tmpsize;

    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return;
    }

    /* Check channel number. */
    if (channel >= SSS_MAX_CHANNELS)
    {
        /* Bogus channel number. */
        return;
    }

    /* Check sample number. */
    if (hsmp >= SSS_MAX_SAMPLES || samples[hsmp].data == NULL ||
            samples[hsmp].smprate == 0)
    {
        /* Bogus sample number. */
        return;
    }

    /*
    ** Stretch the sample from the assumed sampling
    ** rate at which the sample was recorded to the
    ** actual sampling rate that the playback system
    ** is using.
    */
    tmpsize = ((long)samples[hsmp].size *
                            (long)mixrate) /
                            (long)samples[hsmp].smprate;

    /*
    ** Now stretch the size to match the desired
    ** sampling rate specified by the caller.
    */
    tmpsize = tmpsize * (long)pitch / (long)samples[hsmp].smprate;

    chan[channel].vsize = tmpsize;

    /* Start the sample playing. */
    chan[channel].isample = hsmp;
    chan[channel].voffset = 0;

    if (chan[channel].vsize < 1)
    {
        chan[channel].isample = IDLE;
    }
}

/*
** sss_music_flush:
** Removes any loaded song from memory.
**
** Parameters:
**      NONE
**
** Returns:
**      NONE
*/
void
sss_music_flush(void)
{
    UINT    u;

    /* Make sure library was initialized. */
    if (!initialized)
    {
        /* Library not initialized. */
        return;
    }

    /* See if a song is loaded. */
    if (song.npatterns == 0)
        return;

    /* Stop playing music. */
    music_stop();

    /* Discard the sample data. */
    for (u = 0; u < song.nsamples; u++)
    {
        sss_sample_delete(song.samples[u]);
    }

    /* Discard patterns. */
    if (song.patterns != NULL)
    {
        /* Discard each pattern. */
        for (u = 0; u < song.npatterns; u++)
        {
            if (song.patterns[u].steps != NULL)
                free(song.patterns[u].steps);
        }
        free(song.patterns);
    }
    song.patterns = NULL;
    song.npatterns = 0;

    /* Discard order list. */
    if (song.order != NULL)
        free(song.order);
    song.order = NULL;
    song.norder = 0;

    /* Discard samples list. */
    if (song.samples != NULL)
        free(song.samples);
    song.samples = NULL;
    song.nsamples = 0;

    /* Zero the song descriptor, in case we missed something. */
    memset(&song, 0, sizeof(song));
}

/*
** sss_music_create:
** Prepares for the definition of a new song.
** If a song is already loaded, it will be
** discarded.
**
** Parameters:
**      Name            Description
**      ----            -----------
**      npatterns       Number of patterns in song.
**      norder          Number of entries in pattern play order list.
**      nsamples        Number of sound samples in song.
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT
sss_music_create(UINT npatterns, UINT norder, UINT nsamples)
{
    UINT    u;

    if (!initialized)
        return SSSERR_NOT_INITED;

    /* Discard any existing song. */
    music_stop();
    sss_music_flush();

    /* Allocate patterns list. */
    song.patterns = malloc(sizeof(MUSICPATTERN_DESC) * npatterns);
    if (song.patterns == NULL)
        return SSSERR_NO_MEMORY;
    memset(song.patterns, 0, sizeof(MUSICPATTERN_DESC) * npatterns);

    /* Allocate samples list. */
    song.samples = malloc(sizeof(UINT) * nsamples);
    if (song.samples == NULL)
    {
        free(song.patterns);
        song.patterns = NULL;
        return SSSERR_NO_MEMORY;
    }
    memset(song.samples, 0, sizeof(UINT) * nsamples);

    /* Allocate play order list. */
    song.order = malloc(sizeof(UINT) * norder);
    if (song.order == NULL)
    {
        free(song.samples);
        song.samples = NULL;
        free(song.patterns);
        song.patterns = NULL;
        return SSSERR_NO_MEMORY;
    }
    memset(song.order, 0, sizeof(UINT) * norder);

    /* Save sizes. */
    song.npatterns = npatterns;
    song.norder = norder;
    song.nsamples = nsamples;

    /* Set default channel pan positions. */
    for (u = 0; u < SSS_MUSIC_CHANNELS; u++)
    {
        if (u % 2)
            song.pan_pos[u] = SSS_PAN_LEFT;
        else
            song.pan_pos[u] = SSS_PAN_RIGHT;
    }

    return SSSERR_OK;
}

/*
** sss_music_define_order:
** Sets one entry in the pattern play order list.
**
** Parameters:
**      Name            Description
**      ----            -----------
**      iorder          Which entry in playlist to set.
**      ipattern        Pattern to play.
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT
sss_music_define_order(UINT iorder, UINT ipattern)
{
    if (!initialized)
        return SSSERR_NOT_INITED;

    /* Make sure song has been created. */
    if (song.npatterns < 1)
        return SSSERR_BAD_PARAM;

    /* Check for bogus order index. */
    if (iorder >= song.norder)
        return SSSERR_BAD_PARAM;
    if (song.order == NULL)
        return SSSERR_BAD_PARAM;

    /* Check for bogus pattern index. */
    if (ipattern >= song.npatterns)
        return SSSERR_BAD_PARAM;

    /* Set specified play order data. */
    song.order[iorder] = ipattern;

    return SSSERR_OK;
}

/*
** sss_music_define_pattern:
** Specifies the size of one of the patterns in
** the song being created.
**
** Parameters:
**      Name            Description
**      ----            -----------
**      ipattern        Which pattern to set up.
**      nsteps          Number of steps in pattern.
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT
sss_music_define_pattern(UINT ipattern, UINT nsteps)
{
    if (!initialized)
        return SSSERR_NOT_INITED;

    /* Make sure song has been created. */
    if (song.npatterns < 1)
        return SSSERR_BAD_PARAM;

    /* Check for bogus pattern index. */
    if (ipattern >= song.npatterns)
        return SSSERR_BAD_PARAM;

    /* Discard pattern if it is already defined. */
    if (song.patterns[ipattern].steps != NULL)
    {
        free(song.patterns[ipattern].steps);
        song.patterns[ipattern].steps = NULL;
        song.patterns[ipattern].nsteps = 0;
    }

    /* Allocate memory for pattern's steps. */
    song.patterns[ipattern].steps = malloc(sizeof(SSS_STEP_DESC) * nsteps);
    if (song.patterns[ipattern].steps == NULL)
    {
        return SSSERR_NO_MEMORY;
    }
    memset(song.patterns[ipattern].steps, 0, sizeof(SSS_STEP_DESC) * nsteps);

    /* Save step count. */
    song.patterns[ipattern].nsteps = nsteps;

    return SSSERR_OK;
}

/*
** sss_music_define_step:
** Specifies data for one of the steps in a pattern.
**
** Parameters:
**      Name            Description
**      ----            -----------
**      ipattern        Which pattern to modify.
**      istep           Which step to modify.
**      step            Pointer to description of step.
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT
sss_music_define_step(UINT ipattern, UINT istep, const SSS_STEP_DESC *step)
{
    if (!initialized)
        return SSSERR_NOT_INITED;

    /* Make sure song has been created. */
    if (song.npatterns < 1)
        return SSSERR_BAD_PARAM;

    /* Check for bogus pattern index. */
    if (ipattern > song.npatterns)
        return SSSERR_BAD_PARAM;

    /* Check for bogus step index. */
    if (istep > song.patterns[ipattern].nsteps)
        return SSSERR_BAD_PARAM;

    /* Save new step data. */
    song.patterns[ipattern].steps[istep] = *step;

    return SSSERR_OK;
}

/*
** sss_music_define_sample:
** Specifies which sample handle to use for one of the
** samples in the current song.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      isample Index of sample in song.
**      hsmp    Handle of sample (from sss_sample_add).
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT
sss_music_define_sample(UINT isample, UINT hsmp)
{
    if (!initialized)
            return SSSERR_NOT_INITED;

    /* Make sure song has been created. */
    if (song.npatterns < 1)
        return SSSERR_BAD_PARAM;

    /* Check for bogus sample index. */
    if (isample > song.nsamples)
        return SSSERR_BAD_PARAM;

    /* Check for bogus sample handle. */
    if (hsmp >= SSS_MAX_SAMPLES)
        return SSSERR_BAD_PARAM;
    if (samples[hsmp].data == NULL)
        return SSSERR_BAD_PARAM;

    /* Save it. */
    song.samples[isample] = hsmp;

    return SSSERR_OK;
}

/*
** sss_music_define_pan:
** Specifies the initial stereo pan position for
** one of the audio channels used for music.
** Whenever the song is started, the channels
** will be panned to these positions.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      ch      Music channel to set pan position for,
**              0..SSS_MUSIC_CHANNELS-1.
**      pan     Pan position (see sss.h for constants).
**
** Returns:
**      NONE
*/
void
sss_music_define_pan(UINT ch, UINT pan)
{
    if (ch >= SSS_MUSIC_CHANNELS)
        return;
    if (pan > SSS_PAN_RIGHT)
        return;
    song.pan_pos[ch] = pan;
}

/*
** sss_music_command:
** Instructs the music system on what to do.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      cmd     Command for music system.
**              See constants in sss.h
**
** Returns:
**      NONE
*/
void
sss_music_command(UINT cmd)
{
    UINT    u;

    if (!initialized)
        return;

    switch(cmd)
    {
        case SSS_CMD_MUSIC_PLAY:
            if (song.npatterns < 1)
                break;
            music_play();
            break;

        case SSS_CMD_MUSIC_STOP:
            if (song.npatterns < 1)
                break;
            music_stop();
            break;

        case SSS_CMD_MUSIC_PAUSE:
            if (song.npatterns < 1)
                break;
            song.playmode = PLAYMODE_PAUSED;

            /* Silence channels that were used for music. */
            for (u = 0; u < SSS_MUSIC_CHANNELS; u++)
            {
                sss_channel_stop(SSS_MUSIC_FIRST + u);
            }

            break;

        case SSS_CMD_MUSIC_REWIND:
            if (song.npatterns < 1)
                break;
            song.playmode = PLAYMODE_REWINDING;
            break;

        case SSS_CMD_MUSIC_FASTFORWARD:
            if (song.npatterns < 1)
                break;
            song.playmode = PLAYMODE_FASTFORWARDING;
            break;
    }
}

/*
** sss_music_state:
** Retrieves the current state of the music system.
**
** Parameters:
**      NONE
**
** Returns:
**      See SSS_STATE_MUSIC_... constants in sss.h
*/
UINT
sss_music_state(void)
{
    UINT state = SSS_STATE_MUSIC_STOPPED;

    /* Determine current state of music system. */
    if (song.playmode == PLAYMODE_PLAYING)
        state = SSS_STATE_MUSIC_PLAYING;
    else if (song.playmode == PLAYMODE_STOPPED)
        state = SSS_STATE_MUSIC_STOPPED;
    else if (song.playmode == PLAYMODE_PAUSED)
        state = SSS_STATE_MUSIC_PAUSED;
    else if (song.playmode == PLAYMODE_REWINDING)
        state = SSS_STATE_MUSIC_REWINDING;
    else if (song.playmode == PLAYMODE_FASTFORWARDING)
        state = SSS_STATE_MUSIC_FASTFORWARDING;

    if (song.npatterns < 1)
        state = SSS_STATE_MUSIC_NOSONGLOADED;

    return state;
}

/*
** sss_music_get_position:
** Retrieves information about the current playback position
** in the song currently being played.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      ipat    Pointer to UINT to return current pattern number.
**      istep   Pointer to UINT to return current step number.
**      iorder  Pointer to UINT to return current play order index.
**      norder  Pointer to UINT to return total entries in play order.
**      rawpos  Pointer to DWORD to return raw song position in.
**
** Returns:
**      NONE
*/
void
sss_music_get_position(UINT *ipat, UINT *istep, UINT *iorder, UINT *norder,
                        DWORD *rawpos)
{
    if (!initialized)
        return;

    if (ipat != NULL)
        *ipat = song.ipattern;
    if (istep != NULL)
        *istep = song.istep;
    if (iorder != NULL)
        *iorder = song.iorder;
    if (norder != NULL)
        *norder = song.norder;
    if (rawpos != NULL)
        *rawpos = song.song_pos;
}

