#CC=gcc
#LD=ld
INCLUDES=`pkg-config --cflags gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 gstreamer-interfaces-0.10 libcrypto libusb-1.0 openssl`-I$(TOP)/jni 
CFLAGS= -g -O1 -DDEBUG -std=gnu89 -fno-omit-frame-pointer
CPPFLAGS=-fPIC
LFLAGS= -g `pkg-config --libs gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 gstreamer-interfaces-0.10 libcrypto libusb-1.0 openssl` -lSDL
TOP = ..
SRCS =	$(TOP)/jni/hu_aad.c	\
	$(TOP)/jni/hu_aap.c	\
        $(TOP)/jni/hu_ssl.c	\
	$(TOP)/jni/hu_usb.c	\
	$(TOP)/jni/hu_uti.c	\
	$(TOP)/jni/c_utils.c	\
	main.c
OBJS = $(SRCS:.c=.o)
APP = headunit

.PHONY: depend clean

all: $(APP)

$(APP): $(OBJS)
	$(CC) -o $(APP) $(OBJS) $(LFLAGS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f $(OBJS) *~ $(APP)

depend: $(SRCS)
	makedepend $^

# DO NOT DELETE
