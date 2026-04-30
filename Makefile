# Makefile for gemray 1.2.0 under FLTK 1.4.4
#
# Windows Makefile for gcc/msys64/mingw64 using gnu make 4.4.1 installed by Chocolatey.
#
# It will run from a Windows command prompt if you have some basic unix commands like mv and rm
#
# This is a very simplistic makefile that relies on FLTK. It recompiles everything, whether changed
# or not. This is probably just fine on a reasonably fast 2025 computer. On an older or slower system
# you will probably want to spell out the .o, .h and .xpm dependencies.
#
# Your FLTK directory should have a working fltk-config script, lib and include subdirectories, etc.
#
FLTKDIR=/Users/Robert/fltk-1.4.4/Release
SRCFILES=gemray.cpp engine.cpp raytracer.cpp stone.cpp Fl_Image_Button.cpp 
CFLAGS=-O3
EXEEXT=.exe

TARGET=gemray$(EXEEXT)

$(TARGET) : $(SRCFILES)
	$(FLTKDIR)/fltk-config --use-images --compile $(SRCFILES) $(CFLAGS)

clean :
	-rm -f *.o $(TARGET)
