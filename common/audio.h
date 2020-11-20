#pragma once

#include <glib.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <thread>

#include "hu_uti.h"
#include "hu_aap.h"

class AudioOutput
{
    snd_pcm_t* aud_handle = nullptr;
    snd_pcm_t* au1_handle = nullptr;

    void MediaPacket(snd_pcm_t* pcm, const byte * buf, int len);
public:
    AudioOutput(const char* outDev = "default");
    ~AudioOutput();

    void MediaPacketAUD(uint64_t timestamp, const byte * buf, int len);
    void MediaPacketAU1(uint64_t timestamp, const byte * buf, int len);
};

class MicInput
{
    std::string micDevice;
    std::thread mic_readthread;
    int cancelPipeRead = -1, cancelPipeWrite = -1;

    snd_pcm_sframes_t read_mic_cancelable(snd_pcm_t* mic_handle, void *buffer, snd_pcm_uframes_t size, bool* canceled);
    void MicThreadMain(IHUAnyThreadInterface* threadInterface);
public:
    MicInput(const std::string& micDevice = "default");
    ~MicInput();

    void Start(IHUAnyThreadInterface* threadInterface);
    void Stop();
};
