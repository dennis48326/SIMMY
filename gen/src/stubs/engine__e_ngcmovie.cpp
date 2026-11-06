// GENERATED stubs for unported functions of engine/e_ngcmovie.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN InitAllMessageQueue() { SIMMY_STUB("InitAllMessageQueue(void)", 0x803365C0); return {}; }
bool WaitUntilPrepare() { SIMMY_STUB("WaitUntilPrepare(void)", 0x80336678); return {}; }
void PrepareReady(bool) { SIMMY_STUB("PrepareReady(bool)", 0x803366BC); }
void PlayControl(unsigned long) { SIMMY_STUB("PlayControl(unsigned long)", 0x803366EC); }
RET_UNKNOWN ProperTimingForStart() { SIMMY_STUB("ProperTimingForStart(void)", 0x80336938); return {}; }
RET_UNKNOWN ProperTimingForGettingNextFrame() { SIMMY_STUB("ProperTimingForGettingNextFrame(void)", 0x80336990); return {}; }
void PushUsedTextureSet(void *) { SIMMY_STUB("PushUsedTextureSet(void *)", 0x80336ABC); }
RET_UNKNOWN PopUsedTextureSet() { SIMMY_STUB("PopUsedTextureSet(void)", 0x80336AEC); return {}; }
void THPAudioMixCallback() { SIMMY_STUB("THPAudioMixCallback(void)", 0x80336B2C); }
void MixAudio(short *, short *, unsigned int) { SIMMY_STUB("MixAudio(short *, short *, unsigned int)", 0x80336CC8); }
RET_UNKNOWN THPPlayerSetVolume(int, int) { SIMMY_STUB("THPPlayerSetVolume(int, int)", 0x80337094); return {}; }
RET_UNKNOWN THPPlayerGetVolume() { SIMMY_STUB("THPPlayerGetVolume(void)", 0x803371C0); return {}; }
bool CreateReadThread(long) { SIMMY_STUB("CreateReadThread(long)", 0x803371F8); return {}; }
void ReadThreadStart() { SIMMY_STUB("ReadThreadStart(void)", 0x803372A4); }
void ReadThreadCancel() { SIMMY_STUB("ReadThreadCancel(void)", 0x803372D8); }
void Reader(void *) { SIMMY_STUB("Reader(void *)", 0x80337314); }
RET_UNKNOWN PopReadedBuffer() { SIMMY_STUB("PopReadedBuffer(void)", 0x803373F4); return {}; }
void PushReadedBuffer(void *) { SIMMY_STUB("PushReadedBuffer(void *)", 0x80337428); }
RET_UNKNOWN PopFreeReadBuffer() { SIMMY_STUB("PopFreeReadBuffer(void)", 0x80337458); return {}; }
void PushFreeReadBuffer(void *) { SIMMY_STUB("PushFreeReadBuffer(void *)", 0x8033748C); }
RET_UNKNOWN PopReadedBuffer2() { SIMMY_STUB("PopReadedBuffer2(void)", 0x803374BC); return {}; }
void PushReadedBuffer2(void *) { SIMMY_STUB("PushReadedBuffer2(void *)", 0x803374F0); }
bool CreateAudioDecodeThread(long, unsigned char *) { SIMMY_STUB("CreateAudioDecodeThread(long, unsigned char *)", 0x80337520); return {}; }
void AudioDecodeThreadStart() { SIMMY_STUB("AudioDecodeThreadStart(void)", 0x803375D4); }
void AudioDecodeThreadCancel() { SIMMY_STUB("AudioDecodeThreadCancel(void)", 0x80337608); }
void AudioDecoder(void *) { SIMMY_STUB("AudioDecoder(void *)", 0x80337644); }
void AudioDecoderForOnMemory(void *) { SIMMY_STUB("AudioDecoderForOnMemory(void *)", 0x8033766C); }
void AudioDecode(THPReadBuffer *) { SIMMY_STUB("AudioDecode(THPReadBuffer *)", 0x80337710); }
RET_UNKNOWN PopFreeAudioBuffer() { SIMMY_STUB("PopFreeAudioBuffer(void)", 0x803377D0); return {}; }
void PushFreeAudioBuffer(void *) { SIMMY_STUB("PushFreeAudioBuffer(void *)", 0x80337804); }
RET_UNKNOWN PopDecodedAudioBuffer(int) { SIMMY_STUB("PopDecodedAudioBuffer(int)", 0x80337834); return {}; }
void PushDecodedAudioBuffer(void *) { SIMMY_STUB("PushDecodedAudioBuffer(void *)", 0x80337874); }
bool CreateVideoDecodeThread(long, unsigned char *) { SIMMY_STUB("CreateVideoDecodeThread(long, unsigned char *)", 0x803378A4); return {}; }
void VideoDecodeThreadStart() { SIMMY_STUB("VideoDecodeThreadStart(void)", 0x8033795C); }
void VideoDecodeThreadCancel() { SIMMY_STUB("VideoDecodeThreadCancel(void)", 0x80337990); }
void VideoDecoder(void *) { SIMMY_STUB("VideoDecoder(void *)", 0x803379CC); }
void VideoDecoderForOnMemory(void *) { SIMMY_STUB("VideoDecoderForOnMemory(void *)", 0x80337AB0); }
void VideoDecode(THPReadBuffer *) { SIMMY_STUB("VideoDecode(THPReadBuffer *)", 0x80337C0C); }
RET_UNKNOWN PopFreeTextureSet() { SIMMY_STUB("PopFreeTextureSet(void)", 0x80337D34); return {}; }
void PushFreeTextureSet(void *) { SIMMY_STUB("PushFreeTextureSet(void *)", 0x80337D68); }
RET_UNKNOWN PopDecodedTextureSet(int) { SIMMY_STUB("PopDecodedTextureSet(int)", 0x80337D98); return {}; }
void PushDecodedTextureSet(void *) { SIMMY_STUB("PushDecodedTextureSet(void *)", 0x80337DD8); }
