/*
--------------------------------------------------------------------

sss_mod.c

C functions for loading Amiga MOD music files on Windows.

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

NOTES

Amiga MOD Music File Format
---------------------------

First 20 bytes:
        ASCII name of song; null-terminated

30 bytes for each instrument (30 * 31 instruments = 930 bytes total):
        22 bytes:  instrument name; null-terminated.
        2 bytes:  byte-swapped integer; number of 16-bit words of
                sample data for this instrument.
        1 byte:  fine-tuning; valid values are -8 to +8.
        1 byte:  volume; valid values are 0 to 64.
        2 bytes:  byte-swapped integer; word offset into sample data
                where repetition of the sample begins.
        2 bytes:  byte-swapped integer; number of words to repeat
                when repeating the sample.

1 byte:
        Number of patterns to play

1 byte:
        CIAA speed (Amiga specific)

128 bytes "Arrangement":
        Pattern number of each pattern to be played

4 bytes:
        Signature, either "M.K." or "FLT4"

1024 bytes for each pattern:
        Note that number of patterns must be calculated by
        taking the file size minus the header size and sample
        size and dividing by 1024.
        The pattern data is 64 notes, where each note is a
        four byte value, as follows:

                byte 0   byte 1   byte 2   byte 3
                -------- -------- -------- --------
                IIIIPPPP PPPPPPPP IIIIEEEE AAAAAAAA

        Where:
                I = 8 bits of instrument number
                P = 12 bits of note pitch
                E = effect number
                A = effect arguments

        Valid effect numbers include:

                0 =     Arpeggiation:  plays the note rapidly
                        at three different pitches.  The low
                        nibble of "A" specifies the first interval
                        in half-steps.  The high nibble of "A"
                        specifies the second interval.

                1 =     Slide up:  Raises the note pitch during
                        playback.  "A" specifies how fast to
                        raise the pitch.

                2 =     Slide down:  Lowers the note pitch during
                        playback.  "A" specifies how fast to
                        lower the pitch.

                3 =     Slide to note:  Raises or lowers the note
                        pitch to a particular note during playback.
                        "A" specifies how fast to change the pitch.

                4 =     Vibrato:  Produces a vibrato effect.
                        The low nibble of "A" specifies the rate
                        and the high niblle of "A" specifies the
                        depth.

                10 =    Volume slide:  Raises or lowers the
                        volume of the note during playback.
                        If the high nibble of "A" is zero, then
                        the low nibble specifies how fast to
                        raise the volume.  If the low nibble
                        of "A" is zero, then the high nibble
                        specifies how fast to lower the volume.

                11 =    Position jump:  Stops playing the
                        current pattern and continues with
                        part "A" of the arrangement.

                12 =    Set volume:  Sets the loudness of
                        the note, where "A" is the loudness
                        from 0 to 64.

                13 =    Pattern break:  Stops the current
                        pattern and continues to the next
                        pattern.  "A" is ignored.  This effect
                        is used when the pattern is less than
                        64 notes long.

                15 =    Set speed:  Sets the playback speed
                        for the song.  "A" specifies a speed
                        from 0 to 31.

                Certain MOD editors/players have additional
                effects that vary by implementation.

N bytes sample data:
        The remainder of the MOD file is the PCM data
        for the audio samples.

--------------------------------------------------------------------
*/

#define STRICT
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "sss.h"

/* Valid signatures for 31-instrument MOD file headers. */
#define MOD_SIGNATURE1  "M.K."
#define MOD_SIGNATURE2  "FLT4"

/* Number of channels in MOD file. */
#define NUM_TRACKS      4

/* The rate at which the samples in the MOD were recorded. */
#define MOD_RECORDED_RATE       8000

/* How much to scale MOD note pitches to match our pitches. */
#define PITCH_SCALE             18

#pragma pack(1)

/* Description of a note. */
typedef struct
{
    unsigned char   b1;     /* High bits of inst # and high bits of pitch */
    unsigned char   b2;     /* Low bits of pitch */
    unsigned char   b3;     /* Low bits of inst # and 4-bits of effect type */
    unsigned char   b4;     /* Effect parameters */
} NOTE_DESC;

/* Description of a pattern. */
typedef struct
{
    NOTE_DESC notes[NUM_TRACKS * 64];       /* Four tracks of 64 notes. */
} PATTERN_DESC;

/* Description of one instrument in a MOD file. */
typedef struct
{
    char                    name[22];       /* Text name of instrument. */
    unsigned short int      length;         /* # words of instrument data. */
    unsigned char           fine_tune;      /* -8..+8 fine tuning of instrument. */
    unsigned char           volume;         /* Loudness 0..64 */
    unsigned short int      repeat_start;   /* Offset where repetition starts. */
    unsigned short int      repeat_length;  /* Length of repetition. */
} INST_HEADER;

/* Header of 31-instrument MOD file. */
typedef struct
{
    char            name[20];       /* Text name of song. */
    INST_HEADER     inst[31];       /* Information about each instrument. */
    unsigned char   num_pats;       /* Number of patterns to play. */
    unsigned char   ciaa_speed;     /* Amiga-specific. */
    unsigned char   pat_order[128]; /* Order in which patterns are played. */
    unsigned char   signature[4];   /* "M.K." or "FLT4". */
} MOD_HEADER;

/* Header of old 15-instrument MOD file. */
typedef struct
{
    char            name[20];       /* Text name of song. */
    INST_HEADER     inst[15];       /* Information about each instrument. */
    unsigned char   num_pats;       /* Number of patterns to play. */
    unsigned char   ciaa_speed;     /* Amiga-specific. */
    unsigned char   pat_order[128]; /* Order in which patterns are played. */
} OLD_MOD_HEADER;

#pragma pack()

/* Header data from MOD file. */
static MOD_HEADER       hdr31;
static OLD_MOD_HEADER   hdr15;

/* Temporary storage for a pattern from MOD file. */
static PATTERN_DESC     modpattern;

/************************* LOCAL FUNCTIONS ************************/

/*
** Swaps the two bytes of a 16-bit word.
*/
static void byteswap(unsigned short int *w)
{
    *w = (((*w) << 8) & 0xFF00) + (((*w) >> 8) & 0xFF);
}

/*
** load15:
** Loads an old-style 15-instrument MOD file.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      fh      File handle of input file.
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT load15(int fh)
{
    OLD_MOD_HEADER  *hdr = &hdr15;
    UINT            u;
    long            filesize;
    long            ltmp;
    UINT            npats;
    UINT            ipat;
    UINT            istep;
    UINT            isample;
    LPSTR           smpdata;
    UINT            hsmp;
    UINT            ichannel;
    SSS_STEP_DESC   dstep;
    NOTE_DESC       modnote;
    UINT            instrument;
    UINT            pitch;

    /* Determine file size. */
    filesize = _llseek(fh, 0L, 2);
    _llseek(fh, 0L, 0);

    /* Read the header. */
    if (_lread(fh, hdr, sizeof(hdr15)) != sizeof(hdr15))
    {
        /* Failed reading from file. */
        _lclose(fh);
        return SSSERR_READ_FILE;
    }

    /* Do byte swapping on integer fields in instrument descriptors. */
    for (u = 0; u < 15; u++)
    {
        byteswap(&hdr->inst[u].length);
        byteswap(&hdr->inst[u].repeat_start);
        byteswap(&hdr->inst[u].repeat_length);
        hdr->inst[u].name[21] = '\0';
        if (hdr->inst[u].repeat_length < 3)
            hdr->inst[u].repeat_length = 0;
    }

    /* Calculate number of patterns in MOD file. */
    ltmp = filesize - sizeof(OLD_MOD_HEADER);
    for (u = 0; u < 15; u++)
        ltmp -= (long)hdr->inst[u].length * 2;
    npats = (UINT)(ltmp / sizeof(PATTERN_DESC));

    /* Start creation of song. */
    if (sss_music_create(npats, hdr->num_pats, 15) != SSSERR_OK)
    {
        return SSSERR_NO_MEMORY;
    }
    for (ipat = 0; ipat < npats; ipat++)
    {
        if (sss_music_define_pattern(ipat, 64) != SSSERR_OK)
            return SSSERR_NO_MEMORY;
    }
    for (u = 0; u < hdr->num_pats; u++)
    {
        sss_music_define_order(u, hdr->pat_order[u]);
    }

    /* Process each pattern in the file. */
    for (ipat = 0; ipat < npats; ipat++)
    {
        /* Read pattern from file. */
        if (_lread(fh, &modpattern, sizeof(modpattern)) != sizeof(modpattern))
        {
            return SSSERR_READ_FILE;
        }

        /* Process the steps in the pattern. */
        for (istep = 0; istep < 64; istep++)
        {
            /* Process each channel for this step. */
            memset(&dstep, 0, sizeof(dstep));
            for (ichannel = 0; ichannel < 4; ichannel++)
            {
                /* Get note play data. */
                modnote = modpattern.notes[ichannel + istep * NUM_TRACKS];
                instrument = ((UINT)modnote.b3 / 16) & 0x0F;
                pitch = ((UINT)modnote.b1 * 256) + (UINT)modnote.b2;
                pitch *= PITCH_SCALE;
                if (instrument > 0 && pitch > 0)
                {
                    dstep.note_pitch[ichannel] = pitch;
                    dstep.note_sample[ichannel] = instrument - 1;
                }

                /* Get effect data. */
                switch(modnote.b3 & 0x0F)
                {
                    case 11: /* Position jump */
                        dstep.note_effect[ichannel] = SSS_EFFECT_JUMP;
                        dstep.note_eparam[ichannel] = (UINT)modnote.b4;
                        break;

                    case 12: /* Set volume */
                        dstep.note_effect[ichannel] = SSS_EFFECT_SET_VOLUME;
                        dstep.note_eparam[ichannel] = (UINT)modnote.b4;
                        break;

                    case 13: /* Pattern break */
                        dstep.note_effect[ichannel] = SSS_EFFECT_PATTERN_BREAK;
                        break;

                    case 15: /* Set speed */
                        dstep.note_effect[ichannel] = SSS_EFFECT_SET_TEMPO;
                        dstep.note_eparam[ichannel] = (UINT)modnote.b4;
                        break;

                    case 0: /* Arpeggiation */
                    case 1: /* Slide up */
                    case 2: /* Slide down */
                    case 3: /* Slide to note */
                    case 4: /* Vibrato */
                    case 10: /* Volume slide */
                        /* NOT IMPLEMENTED YET */
                        break;

                    default:
                        ; /* Unsupported */
                }
            }
            sss_music_define_step(ipat, istep, &dstep);
        }
    }

    /* Load the samples. */
    for (isample = 0; isample < 15; isample++)
    {
        /* Allocate temporary memory for sample data. */
        smpdata = malloc(hdr->inst[isample].length * 2);
        if (smpdata == NULL)
        {
            return SSSERR_NO_MEMORY;
        }

        /* Load the sample data. */
        if (_lread(fh, smpdata, hdr->inst[isample].length * 2) !=
                                (unsigned)hdr->inst[isample].length * 2)
        {
            free(smpdata);
            return SSSERR_READ_FILE;
        }

        /* Define the sample. */
        hsmp = sss_sample_add(smpdata,
                        hdr->inst[isample].length * 2,
                        hdr->inst[isample].repeat_start * 2,
                        hdr->inst[isample].repeat_length * 2,
                        MOD_RECORDED_RATE, 0);
        if (hsmp >= SSS_MAX_SAMPLES)
        {
            free(smpdata);
            return hsmp;
        }
        sss_music_define_sample(isample, hsmp);

        /* Discard temporary sample buffer. */
        free(smpdata);
    }

    return SSSERR_OK;
}

/*
** load31:
** Loads a 31-instrument MOD file.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      fh      File handle of input file.
**
** Returns:
**      See SSSERR_... constants in sss.h
*/
UINT load31(int fh)
{
    MOD_HEADER      *hdr = &hdr31;
    UINT            u;
    long            filesize;
    long            ltmp;
    UINT            npats;
    UINT            ipat;
    UINT            istep;
    UINT            isample;
    LPSTR           smpdata;
    UINT            hsmp;
    UINT            ichannel;
    SSS_STEP_DESC   dstep;
    NOTE_DESC       modnote;
    UINT            instrument;
    UINT            pitch;

    /* Determine file size. */
    filesize = _llseek(fh, 0L, 2);
    _llseek(fh, 0L, 0);

    /* Read the header. */
    if (_lread(fh, hdr, sizeof(hdr31)) != sizeof(hdr31))
    {
        /* Failed reading from file. */
        _lclose(fh);
        return SSSERR_READ_FILE;
    }

    /* Do byte swapping on integer fields in instrument descriptors. */
    for (u = 0; u < 31; u++)
    {
        byteswap(&hdr->inst[u].length);
        byteswap(&hdr->inst[u].repeat_start);
        byteswap(&hdr->inst[u].repeat_length);
        hdr->inst[u].name[21] = '\0';
        if (hdr->inst[u].repeat_length < 3)
            hdr->inst[u].repeat_length = 0;
    }

    /* Calculate number of patterns in MOD file. */
    ltmp = filesize - sizeof(MOD_HEADER);
    for (u = 0; u < 31; u++)
        ltmp -= (long)hdr->inst[u].length * 2;
    npats = (UINT)(ltmp / sizeof(PATTERN_DESC));

    /* Start creation of song. */
    if (sss_music_create(npats, hdr->num_pats, 31) != SSSERR_OK)
    {
        return SSSERR_NO_MEMORY;
    }
    for (ipat = 0; ipat < npats; ipat++)
    {
        if (sss_music_define_pattern(ipat, 64) != SSSERR_OK)
            return SSSERR_NO_MEMORY;
    }
    for (u = 0; u < hdr->num_pats; u++)
    {
        sss_music_define_order(u, hdr->pat_order[u]);
    }

    /* Process each pattern in the file. */
    for (ipat = 0; ipat < npats; ipat++)
    {
        /* Read pattern from file. */
        if (_lread(fh, &modpattern, sizeof(modpattern)) != sizeof(modpattern))
        {
            return SSSERR_READ_FILE;
        }

        /* Process the steps in the pattern. */
        for (istep = 0; istep < 64; istep++)
        {
            /* Process each channel for this step. */
            memset(&dstep, 0, sizeof(dstep));
            for (ichannel = 0; ichannel < 4; ichannel++)
            {
                /* Get note data. */
                modnote = modpattern.notes[ichannel + istep * NUM_TRACKS];
                instrument =
                        ((UINT)modnote.b1 & 0xF0) +
                        (((UINT)modnote.b3 / 16) & 0x0F);
                pitch = (((UINT)modnote.b1 & 0x0F) * 256) +
                                (UINT)modnote.b2;
                pitch *= PITCH_SCALE;
                if (instrument > 0 && pitch > 0)
                {
                    dstep.note_pitch[ichannel] = pitch;
                    dstep.note_sample[ichannel] = instrument - 1;
                }

                /* Get effect data. */
                switch(modnote.b3 & 0x0F)
                {
                    case 11: /* Position jump */
                        dstep.note_effect[ichannel] = SSS_EFFECT_JUMP;
                        dstep.note_eparam[ichannel] = (UINT)modnote.b4;
                        break;

                    case 12: /* Set volume */
                        dstep.note_effect[ichannel] = SSS_EFFECT_SET_VOLUME;
                        dstep.note_eparam[ichannel] = (UINT)modnote.b4;
                        break;

                    case 13: /* Pattern break */
                        dstep.note_effect[ichannel] = SSS_EFFECT_PATTERN_BREAK;
                        break;

                    case 15: /* Set speed */
                        dstep.note_effect[ichannel] = SSS_EFFECT_SET_TEMPO;
                        dstep.note_eparam[ichannel] = (UINT)modnote.b4;
                        break;

                    case 0: /* Arpeggiation */
                    case 1: /* Slide up */
                    case 2: /* Slide down */
                    case 3: /* Slide to note */
                    case 4: /* Vibrato */
                    case 10: /* Volume slide */
                        /* NOT IMPLEMENTED YET */
                        break;

                    default:
                        ; /* Unsupported */
                }
            }
            sss_music_define_step(ipat, istep, &dstep);
        }
    }

    /* Load the samples. */
    for (isample = 0; isample < 31; isample++)
    {
        /* Allocate temporary memory for sample data. */
        smpdata = malloc(hdr->inst[isample].length * 2);
        if (smpdata == NULL)
        {
            return SSSERR_NO_MEMORY;
        }

        /* Load the sample data. */
        if (_lread(fh, smpdata, hdr->inst[isample].length * 2) !=
                                (unsigned)hdr->inst[isample].length * 2)
        {
            free(smpdata);
            return SSSERR_READ_FILE;
        }

        /* Define the sample. */
        hsmp = sss_sample_add(smpdata,
                        hdr->inst[isample].length * 2,
                        hdr->inst[isample].repeat_start * 2,
                        hdr->inst[isample].repeat_length * 2,
                        MOD_RECORDED_RATE, 0);
        if (hsmp >= SSS_MAX_SAMPLES)
        {
            free(smpdata);
            return hsmp;
        }
        sss_music_define_sample(isample, hsmp);

        /* Discard temporary sample buffer. */
        free(smpdata);
    }

    return SSSERR_OK;
}

/**************************** FUNCTIONS ***************************/

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
**      See SSSERR_... constants in sss.h
*/
UINT
sss_music_load_mod(LPSTR fn)
{
    int     fh;     /* File handle to input file. */
    UINT    result;

    /* Open the input file. */
    fh = _lopen(fn, OF_READ);
    if (fh < 0)
    {
        /* Failed opening file. */
        return SSSERR_OPEN_FILE;
    }

    /* Read the header. */
    if (_lread(fh, &hdr31, sizeof(hdr31)) != sizeof(hdr31))
    {
        /* Failed reading from file. */
        _lclose(fh);
        return SSSERR_READ_FILE;
    }
    _llseek(fh, 0L, 0);

    /* Determine if it's a 15-instrument or 31-instrument MOD file. */
    if (strncmp((const char *)hdr31.signature, MOD_SIGNATURE1, strlen(MOD_SIGNATURE1)) != 0 &&
            strncmp((const char *)hdr31.signature, MOD_SIGNATURE2, strlen(MOD_SIGNATURE2)) != 0)
    {
        /*
        ** It's probably an old-style 15-instrument MOD file.
        */

        result = load15(fh);
        if (result != SSSERR_OK)
        {
            /* Failed loading file. */
            _lclose(fh);
            sss_music_flush();
            return result;
        }
    }
    else
    {
        /*
        ** It's probably a 31-instrument MOD file.
        */

        result = load31(fh);
        if (result != SSSERR_OK)
        {
            /* Failed loading file. */
            _lclose(fh);
            sss_music_flush();
            return result;
        }
    }

    /* Close the input file. */
    _lclose(fh);

    /* Set initial pan positions for MOD. */
    sss_music_define_pan(0, SSS_PAN_LEFT);
    sss_music_define_pan(1, SSS_PAN_RIGHT);
    sss_music_define_pan(2, SSS_PAN_RIGHT);
    sss_music_define_pan(3, SSS_PAN_LEFT);

    return SSSERR_OK;
}

