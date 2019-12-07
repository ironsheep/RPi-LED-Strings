bin_PROGRAMS = matrix

AM_CPPFLAGS = $(INTI_CFLAGS) $(MATRIXDRIVER_CPPFLAGS)

matrix_SOURCES = matrix.c commandProcessor.c  debug.c  frameBuffer.c  imageLoader.c  xmalloc.c matrixDriver.c

#matrix_OBS := 

matrix_LDADD = $(INTI_LIBS)

EXTRA_DIST=matrix.lsm.in matrix.spec.in matrix.texi

#  build and install the .info pages
info_TEXINFOS = matrix.texi
matrix_TEXINFOS = gpl.texi

#  install the man pages
man_MANS=matrix.1
