/*
--------------------------------------------------------------------

sss.h

C header file for Simple Sound System, a multi-channel digital
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

/**************************** CONSTANTS ***************************/

/* Number of volume level setting (range 0 to MAX_VOLUME-1) */
#define SSS_MAX_VOLUME  16

/* Stereo pan positions for audio channels. */
#define SSS_PAN_LEFT    0
#define SSS_PAN_CENTER  (SSS_MAX_VOLUME / 2)
#define SSS_PAN_RIGHT   (SSS_MAX_VOLUME - 1)

/*
**  Number of discreet software audio channels,
**  total of both music and sound effects channels.
*/
#define SSS_MAX_CHANNELS        12

/*
** Number of audio channels used for music.
*/
#define SSS_MUSIC_CHANNELS      8

/*
** First audio channel used for music.
** Prior channels are available for
** sound effects.
*/
#define SSS_MUSIC_FIRST (SSS_MAX_CHANNELS - SSS_MUSIC_CHANNELS)

/*
** Maximum number of samples simultaneously loaded.
*/
#define SSS_MAX_SAMPLES 64

/* Error return codes (must be positive and large values). */
#define SSSERR_OK               0xFFFF  /* No error. */
#define SSSERR_ALREADY_INITED   0xFFFE  /* Can't initialize library twice. */
#define SSSERR_NOT_INITED       0xFFFD  /* Library not initialized. */
#define SSSERR_NO_MEMORY        0xFFFC  /* Out of memory. */
#define SSSERR_NO_HANDLES       0xFFFB  /* No more sample handles. */
#define SSSERR_OPEN_DEVICE      0xFFFA  /* Couldn't open wave out device. */
#define SSSERR_OPEN_CAPS        0xFFF9  /* Couldn't get wave out capabilities. */
#define SSSERR_OPEN_FORMAT      0xFFF8  /* No compatible wave out formats. */
#define SSSERR_NO_TIMER         0xFFF7  /* Out of system timers. */
#define SSSERR_BAD_PARAM        0xFFF6  /* Invalid parameter specified. */
#define SSSERR_OPEN_FILE        0xFFF5  /* Failed opening a file. */
#define SSSERR_READ_FILE        0xFFF4  /* Failed reading from a file. */

/* Types of effects used in steps in a pattern: */
#define SSS_EFFECT_NONE                 0
#define SSS_EFFECT_PATTERN_BREAK        1
#define SSS_EFFECT_JUMP                 2
#define SSS_EFFECT_SET_TEMPO            3
#define SSS_EFFECT_SET_VOLUME           4

/* Commands for the music system, via sss_music_command: */
#define SSS_CMD_MUSIC_PLAY              1
#define SSS_CMD_MUSIC_STOP              2
#define SSS_CMD_MUSIC_PAUSE             3
#define SSS_CMD_MUSIC_REWIND            4
#define SSS_CMD_MUSIC_FASTFORWARD       5

/* States for the music system, via sss_music_state: */
#define SSS_STATE_MUSIC_PLAYING         1
#define SSS_STATE_MUSIC_PAUSED          2
#define SSS_STATE_MUSIC_REWINDING       3
#define SSS_STATE_MUSIC_FASTFORWARDING  4
#define SSS_STATE_MUSIC_STOPPED         5
#define SSS_STATE_MUSIC_NOSONGLOADED    6

/**************************** TYPES *******************************/

/* Struct used to describe one step of music. */
typedef struct
{
    /* Pitch of note to play on each channel (or 0 for none). */
    UINT    note_pitch[SSS_MUSIC_CHANNELS];

    /* Sample index of sample to play on each channel. */
    UINT    note_sample[SSS_MUSIC_CHANNELS];

    /* Type of effect for each channel. */
    UINT    note_effect[SSS_MUSIC_CHANNELS];

    /* Effect params for each channel. */
    UINT    note_eparam[SSS_MUSIC_CHANNELS];
} SSS_STEP_DESC;

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
UINT    sss_init(HINSTANCE hinst);

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
void    sss_deinit(void);

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
UINT    sss_get_mixrate(void);

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
UINT    sss_get_channel_count(void);

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
void    sss_channel_pan_set(UINT channel, UINT pan);

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
UINT    sss_channel_pan_get(UINT channel);

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
UINT    sss_channel_is_busy(UINT channel);

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
void    sss_channel_stop(UINT channel);

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
void    sss_channel_volume(UINT channel, UINT v);

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
**      SSSERR_...      See SSSERR constants above.
**      other           Handle of new sample, a value
**                      from zero to SSS_MAX_SAMPLES-1.
*/
UINT    sss_sample_add(LPSTR data, UINT size,
                UINT loopbeg, UINT loopsiz, UINT smprate, UINT center);

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
void    sss_sample_delete(UINT hsmp);

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
void    sss_sample_play(UINT channel, UINT hsmp, UINT pitch);

/*
** sss_music_command:
** Instructs the music system on what to do.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      cmd     Command for music system.
**              See constants above.
**
** Returns:
**      NONE
*/
void    sss_music_command(UINT cmd);

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
void    sss_music_flush(void);

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
**      See SSSERR_... constants above.
*/
UINT    sss_music_create(UINT npatterns, UINT norder, UINT nsamples);

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
**      See SSSERR_... constants above.
*/
UINT    sss_music_define_order(UINT iorder, UINT ipattern);

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
**      See SSSERR_... constants above.
*/
UINT    sss_music_define_pattern(UINT ipattern, UINT nsteps);

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
**      See SSSERR_... constants above.
*/
UINT    sss_music_define_step(UINT ipattern, UINT istep, const SSS_STEP_DESC *step);

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
**      See SSSERR_... constants above.
*/
UINT    sss_music_define_sample(UINT isample, UINT hsmp);

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
void    sss_music_define_pan(UINT ch, UINT pan);

/*
** sss_music_state:
** Retrieves the current state of the music system.
**
** Parameters:
**      NONE
**
** Returns:
**      See SSSERR_... constants above.
*/
UINT    sss_music_state(void);
void    sss_music_get_position(UINT *ipat, UINT *istep,
                UINT *iorder, UINT *norder, DWORD *rawpos);

/*
** sss_music_load_mod:
** Loads a MOD type music file.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      fn      Pathname of file to load.
**
** Returns:
**      See SSSERR_... constants above.
*/
UINT    sss_music_load_mod(LPSTR fn);

