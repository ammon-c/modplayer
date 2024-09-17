/*
--------------------------------------------------------------------

test.c

Simple command-line program to play an Amiga MOD music file
on Windows from the command line.  Useful mainly for testing
the sound code. 

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

#define STRICT
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>

#include "sss.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage:  test filename.MOD\n");
        return 1;
    }

    printf("Initializing.\n");
    if (sss_init(NULL) != SSSERR_OK)
    {
        printf("sss_init() failed!\n");
        return 1;
    }

    printf("Loading music from \"%s\"\n", argv[1]);
    if (sss_music_load_mod(argv[1]) != SSSERR_OK)
    {
        printf("Failed loading music!\n");
        sss_deinit();
        return 1;
    }

    printf("Playing.  Press a key to stop.\n");
    sss_music_command(SSS_CMD_MUSIC_PLAY);
    while (1)
    {
        if (_kbhit())
        {
            _getch();
            break;
        }
    }

    printf("Cleaning up.\n");
    sss_deinit();
    printf("Exiting.\n");
    return 0;
}

