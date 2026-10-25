// Minimal libnx API test double used only with tests/switch_audio_mock.cpp.
#pragma once
#include <cstdint>
#include <cstddef>
using u32=uint32_t; using u64=uint64_t; using Result=u32;
#define R_FAILED(r) ((r)!=0)
#define R_SUCCEEDED(r) ((r)==0)
#define MAKERESULT(m,d) (u32((m)|((d)<<9)))
enum { Module_Libnx=345, LibnxError_BadInput=11, LibnxError_OutOfMemory=2, LibnxError_IncompatSysVer=39 };
enum PcmFormat { PcmFormat_Int16=2 };
struct AudioOutBuffer { AudioOutBuffer* next; void* buffer; u64 buffer_size,data_size,data_offset; };
Result audoutInitialize(); void audoutExit(); u32 audoutGetSampleRate(); u32 audoutGetChannelCount(); PcmFormat audoutGetPcmFormat();
bool hosversionBefore(int,int,int); void armDCacheFlush(void*,size_t);
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer**,u32*); Result audoutAppendAudioOutBuffer(AudioOutBuffer*);
Result audoutStartAudioOut(); Result audoutStopAudioOut(); Result audoutFlushAudioOutBuffers(bool*);
