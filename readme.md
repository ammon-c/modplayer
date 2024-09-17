## MODPlayer - Play Amiga .MOD music files on Windows (1995)

                          PLEASE NOTE:
          THIS IS AN ARCHIVE OF ONE OF MY OLDER PROJECTS
            THAT HAS NOT BEEN UPDATED IN SEVERAL YEARS

**Description:**

This is the code for a Windows application that can load and play
old Amiga MOD music files.  MOD was one of the earliest computer
music formats to support digital audio samples.  

I originally wrote this program as a 16-bit Windows application
in 1993, then refactored it to a 32-bit application in 1995.  

The audio playback code allows up to 12 channels of digital
audio samples to be mixed in realtime, although only 4 of the
channels are used when playing .MOD files.  The code supports 12
channels because I had also planned to use it in some game
projects where it needed to be able to play MOD music and
several game sound effects at the same time.  

The user interface consists of a simple Windows dialog box with
several pushbuttons for controlling the music playback
functions.  

**Language:**  MODPlayer is written in C

**Platform:**  Windows 32-bit or 64-bit

**Tools:**  Microsoft Visual Studio with C compiler

**Build:**  Run the makefile using NMAKE from the command prompt.
The finished executable in placed in a file named **modplayer.exe**

**Source Code Files:**

* **modplayer.c:** C source for the main module of the music player app.
* **sss.h:** C header for the low-level audio module.
* **sss.c:** C code for the low-level audio module.
* **sss_mod.c:** C functions for reading Amiga MOD files.
* **test.c:** C source for a very simplistic command-line MOD player, for testing the audio module.
* **makefile:** Build script for use with Microsoft NMake.

* The **testdata** subdirectory contains several .MOD music files for testing.

**Limitations:**

* Some of the audio effects that can appear in .MOD files aren't
yet supported by this code (such as vibrato, slides,
arpeggiation, etc).  Files containing such effects may not play
correctly.  

