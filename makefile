####################################################
# FILENAME:     Makefile
# DESCRIPTION:  NMAKE script for building the MOD
#               player code and program.
# AUTHOR:       Ammon R. Campbell
# TOOLS:        Microsoft Visual C++
####################################################

CFLAGS= -nologo -c -EHsc -WX -Gs -Zi -W4 -DWIN32

.SUFFIXES:
.SUFFIXES:   .c

.c.obj:
   cl $(CFLAGS) $*.c

all:   modplayer.exe test.exe

#
# Build the MOD player application from the object files.
#
modplayer.exe:   sss.obj sss_mod.obj modplayer.obj modplayer.res
   if exist link.tmp del link.tmp
   echo /NOLOGO                           >> link.tmp
   echo modplayer.obj                        >> link.tmp
   echo sss.obj                           >> link.tmp
   echo sss_mod.obj                       >> link.tmp
   echo /OUT:$@                           >> link.tmp
   echo /DEBUG                            >> link.tmp
   echo /SUBSYSTEM:WINDOWS                >> link.tmp
   echo /ENTRY:WinMainCRTStartup          >> link.tmp
   echo user32.lib gdi32.lib comdlg32.lib >> link.tmp
   echo shell32.lib advapi32.lib winmm.lib>> link.tmp
   echo modplayer.res                        >> link.tmp
   link /NOLOGO @link.tmp
   if exist link.tmp del link.tmp
   if exist $*.lib del $*.lib
   if exist $*.exp del $*.exp

#
# Build the resource file from the resource script
#
modplayer.res:   modplayer.rc resource.h *.ico
   rc -r -fo$@ modplayer.rc

#
# Build the object files from the C sources
#
modplayer.obj:    modplayer.c resource.h sss.h
test.obj:      test.c sss.h
sss.obj:       sss.c sss.h
sss_mod.obj:   sss_mod.c sss.h

#
# Build the command line test applet
#
test.exe:   test.obj sss.obj sss_mod.obj
   if exist link.tmp del link.tmp
   echo /NOLOGO                           >> link.tmp
   echo test.obj                          >> link.tmp
   echo sss.obj                           >> link.tmp
   echo sss_mod.obj                       >> link.tmp
   echo /OUT:$@                           >> link.tmp
   echo /DEBUG                            >> link.tmp
   echo user32.lib gdi32.lib comdlg32.lib >> link.tmp
   echo shell32.lib advapi32.lib winmm.lib>> link.tmp
   link /NOLOGO @link.tmp
   if exist link.tmp del link.tmp
   if exist $*.lib del $*.lib
   if exist $*.exp del $*.exp

#
# Prepare for a fresh rebuild
#
clean:
   if exist link.tmp del link.tmp
   if exist *.obj del *.obj
   if exist *.lst del *.lst
   if exist *.bak del *.bak
   if exist *.aps del *.aps
   if exist *.res del *.res
   if exist *.map del *.map
   if exist *.exp del *.exp
   if exist *.pdb del *.pdb
   if exist *.ilk del *.ilk
   if exist *.lib del *.lib
   if exist *.exe del *.exe
   if exist *.dll del *.dll

