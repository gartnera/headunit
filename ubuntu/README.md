The following instructions only tested on ubuntu 16.04 and 16.10 x64, however should be working on most distros with up-to-date

# How to run headunit
Install dependencies:

```
sudo apt-get install libsdl2-2.0-0 libsdl2-ttf-2.0-0 libportaudio2 libpng12-0 gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-libav gstreamer1.0-alsa
```

# How to compile headunit for Ubuntu
We need development headers for openssl, libusb, gstreamer, sdl and gkt

```
sudo apt-get install libssl-dev libusb-1.0-0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libsdl1.2-dev libgtk-3-dev libudev-dev libunwind-dev libsdl2-dev libgstreamer-plugins-bad1.0-dev protobuf-compiler
```
