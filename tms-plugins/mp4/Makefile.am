ACLOCAL_AMFLAGS = -I m4
JANUS_PATH=$(exec_prefix)
CFLAGS = $(shell pkg-config --cflags glib-2.0) -lavformat -lavcodec -lavutil -lswresample -lswscale -lavfilter -I$(JANUS_PATH)/include/janus
LIBS = $(shell pkg-config --libs glib-2.0) 

lib_LTLIBRARIES = libjanus_tms_mp4.la
libjanus_tms_mp4_la_SOURCES = janus_plugin_tms_mp4.c tms_ffmpeg_mp4.c
libjanus_tms_mp4_la_LDFLAGS = -version-info 0:0:0 $(shell pkg-config --libs glib-2.0) -lavformat -lavcodec -lavutil -lswresample -lswscale -lavfilter -L$(JANUS_PATH)/lib
libdir = $(exec_prefix)/lib/janus/plugins