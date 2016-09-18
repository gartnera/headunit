#CC=gcc
#LD=ld
INCLUDES=$(shell pkg-config --cflags gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 gstreamer-interfaces-0.10 libcrypto libusb-1.0 openssl) -I$(TOP)/jni 
CFLAGS= -g 
CPPFLAGS=-fPIC
LFLAGS= -g $(shell pkg-config --libs gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 gstreamer-interfaces-0.10 libcrypto libusb-1.0 openssl) -lSDL
TOP = ..
SRCS =	$(TOP)/jni/hu_aad.c	\
	$(TOP)/jni/hu_aap.c	\
        $(TOP)/jni/hu_ssl.c	\
	$(TOP)/jni/hu_usb.c	\
	$(TOP)/jni/hu_uti.c	\
	main.c
OBJS = $(SRCS:.c=.x64.o)
DEPS = $(SRCS:.c=.x64.d)
APP = headunit

.PHONY: clean

all: $(APP)

$(APP): $(OBJS)
	$(CC) -o $(APP) $(OBJS) $(LFLAGS)

%.x64.o : %.c
	$(CC) -MD $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f $(OBJS) $(DEPS) *~ $(APP)

-include $(DEPS)

# DO NOT DELETE
