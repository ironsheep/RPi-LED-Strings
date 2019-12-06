bin_PROGRAMS = src/matrix

AM_CXXFLAGS = $(INTI_CFLAGS)

matrix_SOURCES = src/commandProcessor.c \
src/commandProcessor.h \
src/debug.c \
src/debug.h \
src/frameBuffer.c \
src/frameBuffer.h \
src/imageLoader.c \
src/imageLoader.h \
src/ledScreen.c \
src/ledScreen.h \
src/matrix.c \
src/piSystem.c \
src/piSystem.h \
src/system.h \
src/xmalloc.c \
src/xmalloc.h

#matrix_OBS := 

matrix_LDADD = $(INTI_LIBS)

EXTRA_DIST=matrix.lsm.in matrix.spec.in matrix.texinfo

#  build and install the .info pages
info_TEXINFOS = matrix.texinfo
matrix_TEXINFOS = gpl.texinfo

#  install the man pages
man_MANS=matrix.1
