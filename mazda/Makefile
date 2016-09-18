M3TOOLCHAIN=./m3-toolchain
CC=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-gcc
CXX=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-g++
LD=$(M3TOOLCHAIN)/bin/arm-cortexa9_neon-linux-gnueabi-ld
SYSROOT=$(M3TOOLCHAIN)/arm-cortexa9_neon-linux-gnueabi/sysroot

INCLUDES=$(shell ./pkg-config-wrapper $(SYSROOT) --cflags gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 dbus-1 libusb-1.0  libcrypto openssl) -I$(TOP)/jni -Imazda-connector
CFLAGS=-fPIC -O3 --sysroot=$(SYSROOT) 
CXXFLAGS=-Os --sysroot=$(SYSROOT) -std=c++11 -static-libstdc++

LFLAGS= --sysroot=$(SYSROOT) $(shell ./pkg-config-wrapper $(SYSROOT) --libs gstreamer-0.10 gstreamer-app-0.10 gstreamer-video-0.10 dbus-1 libusb-1.0 libcrypto openssl)
LFLAGS_IF= --sysroot=$(SYSROOT) -static-libstdc++ $(shell ./pkg-config-wrapper $(SYSROOT) --libs dbus-1)
TOP = ..

#headunit binary
SRCS =	$(TOP)/jni/hu_aad.c	\
	$(TOP)/jni/hu_aap.c	\
    $(TOP)/jni/hu_ssl.c	\
	$(TOP)/jni/hu_usb.c	\
	$(TOP)/jni/hu_uti.c	\
	main.c
OBJS = $(SRCS:.c=.arm.o)
DEPS = $(SRCS:.c=.arm.d)

APP = headunit

SRCS_IF = \
	mazda-connector/src/input_filter/input_filter.cpp
OBJS_IF = $(SRCS_IF:.cpp=.arm.o)
DEPS_IF = $(SRCS_IF:.cpp=.arm.d)

#input_filter binary
APP_IF = input_filter

all: $(APP) $(APP_IF)

$(APP): $(OBJS)
	$(CC) -MD -g -o $(APP) $(OBJS) $(LFLAGS)

$(APP_IF): $(OBJS_IF)
	$(CXX) -MD -g -o $(APP_IF) $(OBJS_IF) $(LFLAGS_IF)

%.arm.o : %.c
	$(CC) -g $(CFLAGS) $(INCLUDES) -c $<  -o $@

%.arm.o : %.cpp
	$(CXX) -g $(CXXFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	rm -f *~ $(OBJS) $(OBJS_IF) $(APP) $(APP_IF) $(DEPS) $(DEPS_IF)

install: all
	cp -a -f input_filter installer/config/androidauto/data_persist/dev/bin/
	cp -a -f headunit installer/config/androidauto/data_persist/dev/bin/

-include $(DEPS)
-include $(DEPS_IF)

.PHONY: clean
