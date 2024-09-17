/*
--------------------------------------------------------------------

modplayer.c

Simple application for playing Amiga MOD music files on Windows.

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
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

#include "resource.h"
#include "sss.h"

/* Control ID for "About" menu item in system menu of main dialog. */
#define IDM_ABOUT 12000

static char my_path[128];               /* App's directory. */
static HINSTANCE my_instance = NULL;    /* App's instance handle. */
static char songfile[128];              /* Pathname of song being played. */

/* Text to display for "About" window: */
static char *about_text = "\
Simple MOD Player\n\
Version 1.3\n\
Copyright © 1995 by Ammon R. Campbell.  Not-for-profit\n\
distribution is permitted.  All other rights reserved.\n\
";

/*
** Displays an error message on the screen and waits for
** the user to close it.
*/
static void errmsg(HWND hwnd, LPSTR text)
{
    MessageBox(hwnd, text, "Error", MB_OK | MB_ICONEXCLAMATION);
}

/*
** Moves a window to the center of the screen.
*/
static void center_window(HWND hwnd)
{
    RECT    crect;  /* Size and position of window. */
    int     width;  /* Width of window in pixels. */
    int     height; /* Height of window in pixels. */

    GetWindowRect(hwnd, (LPRECT)&crect);
    width = crect.right - crect.left + 1;
    height = crect.bottom - crect.top + 1;
    MoveWindow(hwnd,
            GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2,
            GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2,
            width,
            height,
            TRUE);
}

/*
** Prompts the user for the name of a music file to open.
** Uses the common "Open File" dialog.
**
** Parameters:
**      Name    Description
**      ----    -----------
**      hwnd    Window handle of parent window.
**      fn      On entry, contains default filename
**              On exit, contains pathname of file specified by user.
**      title   Text to show for title of file prompt dialog.
**
** Returns:
**      Value   Meaning
**      -----   -------
**      1       Successful.
**      0       User cancelled.
*/
static int get_filename(HWND hwnd, char *fn, char *title)
{
    OPENFILENAME    ofn;
    char            out_fn[128];
    char            initial_dir[128];

    /* Split default filename into directory and filename portions. */
    out_fn[0] = '\0';
    strcpy_s(initial_dir, sizeof(initial_dir), fn);
    int i = (int)strlen(initial_dir);
    while (i > 0 && initial_dir[i] != '\\')
        i--;
    if (initial_dir[i] == '\\')
    {
        strcpy_s(out_fn, sizeof(out_fn), &initial_dir[i + 1]);
        initial_dir[i] = '\0';
    }

    /* Prepare struct for common dialog box function. */
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "\
4-Channel MOD files\x00\
*.mod;*.nst;*.mtm\x00\
\x00";
    ofn.lpstrTitle = title;                 /* Dialog title text. */
    ofn.lpstrFile = out_fn;                 /* Filename in / path out. */
    ofn.nMaxFile = 127;                     /* Size of above buffer. */
    ofn.lpstrInitialDir = initial_dir;      /* Initial directory in. */

    /* Run the common dialog. */
    if (!GetOpenFileName(&ofn))
    {
        /* User cancelled. */
        return 0;
    }

    strcpy_s(fn, 128, out_fn);
    return 1;
}

/*
** Message handler for the app's main dialog box.
*/
INT_PTR DlgPlayProc(HWND hdlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    WORD    ctlid;          /* ID of control for WM_COMMAND */
    HWND    ctlwnd;         /* Window for WM_COMMAND */
    WORD    cmd;            /* Notify/submessage for WM_COMMAND */
    char    stmp[128];      /* Temporary text string. */
    char    postext[128];   /* Temporary text string. */
    HMENU   hmenu;          /* Temporary handle to dialog's system menu. */
    UINT    ipat;           /* Temporary current pattern in song. */
    UINT    iorder;         /* Temporary current pattern in sequence. */
    UINT    norder;         /* Temporary number of patterns in sequence. */

    switch (message)
    {
        case WM_TIMER:
            /* Update status display. */
            sss_music_get_position(&ipat, NULL, &iorder, &norder, NULL);
            sprintf_s(postext, sizeof(postext), "   Sequence %03u of %03u   Pattern %03u",
                    iorder, norder, ipat);
            switch(sss_music_state())
            {
                case SSS_STATE_MUSIC_STOPPED:
                    SetDlgItemText(hdlg, IDS_STATUS, "STOPPED");
                    break;
                case SSS_STATE_MUSIC_PLAYING:
                    strcpy_s(stmp, sizeof(stmp), "PLAYING");
                    strcat_s(stmp, sizeof(stmp), postext);
                    SetDlgItemText(hdlg, IDS_STATUS, stmp);
                    break;
                case SSS_STATE_MUSIC_PAUSED:
                    strcpy_s(stmp, sizeof(stmp), "PAUSED");
                    strcat_s(stmp, sizeof(stmp), postext);
                    SetDlgItemText(hdlg, IDS_STATUS, stmp);
                    break;
                case SSS_STATE_MUSIC_REWINDING:
                    strcpy_s(stmp, sizeof(stmp), "REWINDING");
                    strcat_s(stmp, sizeof(stmp), postext);
                    SetDlgItemText(hdlg, IDS_STATUS, stmp);
                    break;
                case SSS_STATE_MUSIC_FASTFORWARDING:
                    strcpy_s(stmp, sizeof(stmp), "FAST FORWARDING");
                    strcat_s(stmp, sizeof(stmp), postext);
                    SetDlgItemText(hdlg, IDS_STATUS, stmp);
                    break;
                case SSS_STATE_MUSIC_NOSONGLOADED:
                    SetDlgItemText(hdlg, IDS_STATUS, "NO SONG LOADED");
                    break;
                default:
                    SetDlgItemText(hdlg, IDS_STATUS, "UNKNOWN STATE");
            }
            return TRUE;

        case WM_INITDIALOG:
            center_window(hdlg);
            SetDlgItemText(hdlg, IDS_FILENAME, songfile);

            /*
            ** If a song was already loaded (i.e. from the
            ** command line), then start playing it now.
            */
            if (songfile[0] != '\0')
                sss_music_command(SSS_CMD_MUSIC_PLAY);

            /* Add the "About" item to the system menu. */
            hmenu = GetSystemMenu(hdlg, FALSE);
            AppendMenu(hmenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hmenu, MF_STRING, IDM_ABOUT, "&About...");

            /* Set a timer to update the window periodically. */
            SetTimer(hdlg, 1, 250, NULL);

            return TRUE;

        case WM_COMMAND:
            /* A dialog control was activated. */
            ctlwnd = (HWND)lparam;
            ctlid = LOWORD(wparam);
            cmd = HIWORD(wparam);
            if (ctlid == IDCANCEL || ctlid == IDOK)
            {
                /* Quit */
                KillTimer(hdlg, 1);
                EndDialog(hdlg, TRUE);
                return TRUE;
            }
            else if (ctlid == IDB_REWIND)
            {
                /* Rewind */
                sss_music_command(SSS_CMD_MUSIC_REWIND);
                return TRUE;
            }
            else if (ctlid == IDB_FASTFORWARD)
            {
                /* Fast Forward */
                sss_music_command(SSS_CMD_MUSIC_FASTFORWARD);
                return TRUE;
            }
            else if (ctlid == IDB_PLAY)
            {
                /* Play Music */
                sss_music_command(SSS_CMD_MUSIC_PLAY);
                return TRUE;
            }
            else if (ctlid == IDB_PAUSE)
            {
                /* Pause Music */
                sss_music_command(SSS_CMD_MUSIC_PAUSE);
                return TRUE;
            }
            else if (ctlid == IDB_STOP)
            {
                 /* Stop Music */
                 sss_music_command(SSS_CMD_MUSIC_STOP);
                 return TRUE;
            }
            else if (ctlid == IDB_OPEN)
            {
                 /* Open a music file. */
                 strcpy_s(stmp, sizeof(stmp), songfile);
                 if (!get_filename(hdlg, stmp, "Open File"))
                     return TRUE;
                 switch(sss_music_load_mod(stmp))
                 {
                     case SSSERR_OK:
                         strcpy_s(songfile, sizeof(songfile), stmp);
                         SetDlgItemText(hdlg, IDS_FILENAME, songfile);
                         sss_music_command(SSS_CMD_MUSIC_PLAY);
                         break;

                     case SSSERR_NO_MEMORY:
                         errmsg(hdlg, "Out of memory");
                         break;

                     case SSSERR_NO_HANDLES:
                         errmsg(hdlg, "File contains too many instruments");
                         break;

                     case SSSERR_OPEN_FILE:
                         errmsg(hdlg, "Failed opening specified file");
                         break;

                     case SSSERR_READ_FILE:
                         errmsg(hdlg, "I/O read failure while reading specified file");
                         break;

                     default:
                         errmsg(hdlg, "Unable to load specified file");
                 }
                 return TRUE;
            }
            break;

        case WM_SYSCOMMAND:
            /* A system control (i.e. system menu) was activated. */
            /* ctlwnd = (HWND)lparam; */
            ctlid = LOWORD(wparam);
            /* cmd = HIWORD(wparam); */
            if (ctlid == IDM_ABOUT)
            {
                MessageBox(hdlg, about_text, "About", MB_OK);
                return TRUE;
            }
            break;
    }

    return FALSE;
}

/*
** init_instance:
** This function is called at init time for each instance of
** this application, and performs initialization tasks that
** are specific to each instance.
**
** Parameters:
**      Name            Description
**      ----            -----------
**      hInstance       The instance handle of this instance
**                      of the application.
**      nCmdShow        nCmdShow flags from WinMain, to be passed
**                      to ShowWindow() after the application's
**                      window is created.
**
** Returns:
**      Value   Meaning
**      -----   -------
**      TRUE    Successful.
**      FALSE   Initialization error occured -- application must be
**              terminated.
*/
static BOOL init_instance(HINSTANCE hInstance, int nCmdShow)
{
    (void)nCmdShow;

    /* Get pathname of application's directory. */
    GetModuleFileName(hInstance, my_path, 127);
    int i = (int)strlen(my_path);
    while (i > 0 && my_path[i] != '\\' && my_path[i] != ':')
        i--;
    if (my_path[i] == '\\' || my_path[i] == ':')
        my_path[i] = '\0';

    /*
    ** In an app with a normal main window, we'd create
    ** the main window here.  Since this app has a
    ** dialog box as the main window, we don't create
    ** one here.
    */

    /*
    ** Start the sound library, and complain if
    ** it reports an error.
    */
    switch(sss_init(my_instance))
    {
        case SSSERR_OK:
            break;

        case SSSERR_OPEN_CAPS:
            errmsg(NULL, "Failed querying wave out device capabilities!");
            DestroyWindow(NULL);
            return FALSE;

        case SSSERR_OPEN_FORMAT:
            errmsg(NULL, "Wave output device does not support any compatible formats!");
            DestroyWindow(NULL);
            return FALSE;

        case SSSERR_OPEN_DEVICE:
            errmsg(NULL, "Can't open wave output device!");
            DestroyWindow(NULL);
            return FALSE;

        default:
            errmsg(NULL, "Unknown error opening wave output device!");
            DestroyWindow(NULL);
            return FALSE;
    }

    return TRUE;
}

/*
** Prepares to shut down the application.
*/
static void deinit_instance(void)
{
    sss_deinit();
}

/*
** Windows application entry point.
*/
int PASCAL WinMain(
    HINSTANCE       hInstance,
    HINSTANCE       hPrevInstance,
    LPSTR           lpCmdLine,
    int             nCmdShow)
{
    if (hPrevInstance)
        return 0;

    my_instance = hInstance;
    if (!init_instance(hInstance, nCmdShow))
        return 0;

    /* If song file was specified on command line, then load it. */
    songfile[0] = '\0';
    if (lpCmdLine[0] != '\0' && lpCmdLine[0] != ' ')
    {
        lstrcpy(songfile, lpCmdLine);
        if (sss_music_load_mod(songfile) != SSSERR_OK)
        {
            char stmp[128];

            sprintf_s(stmp, sizeof(stmp), "Unable to load \"%s\"\n", songfile);
            errmsg(NULL, stmp);
            songfile[0] = '\0';
        }
    }

    /* Display the app's main dialog box. */
    DialogBox(my_instance,
            MAKEINTRESOURCE(ID_DLG_TRANSPORT),
            NULL, DlgPlayProc);

    deinit_instance();
    return 0;
}

