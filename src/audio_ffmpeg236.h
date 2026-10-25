#pragma once
// Bounded in-memory audio decode adapter. No URLs, filesystem protocols, video
// output, network access or audio threads are exposed to a MIDlet by this module.
// API usage follows FFmpeg 7.1's public AVIO and send_packet/receive_frame APIs.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
extern "C" {
#include "../third_party/ffmpeg_headers/libavcodec/avcodec.h"
#include "../third_party/ffmpeg_headers/libavformat/avformat.h"
#include "../third_party/ffmpeg_headers/libavutil/avutil.h"
#include "../third_party/ffmpeg_headers/libavutil/mem.h"
#include "../third_party/ffmpeg_headers/libavutil/samplefmt.h"
}
#ifndef __SWITCH__
#include <dlfcn.h>
#endif
namespace audio_ffmpeg236 {
#define J2ME_FF_FUNCTIONS(X) \
 X(avutil_version) X(avcodec_version) X(avformat_version) \
 X(av_malloc) X(av_free) X(av_dict_set) X(av_dict_free) \
 X(av_frame_alloc) X(av_frame_free) X(av_frame_unref) \
 X(av_get_bytes_per_sample) X(av_sample_fmt_is_planar) X(av_get_packed_sample_fmt) \
 X(avio_alloc_context) X(avio_context_free) X(avformat_alloc_context) \
 X(avformat_open_input) X(avformat_find_stream_info) X(avformat_close_input) \
 X(av_find_best_stream) X(av_read_frame) X(avcodec_find_decoder) \
 X(avcodec_alloc_context3) X(avcodec_parameters_to_context) X(avcodec_open2) \
 X(avcodec_free_context) X(avcodec_send_packet) X(avcodec_receive_frame) \
 X(av_packet_alloc) X(av_packet_free) X(av_packet_unref)
struct Api {
#define FF_DECLARE(n) decltype(&::n) p_##n=nullptr;
 J2ME_FF_FUNCTIONS(FF_DECLARE)
#undef FF_DECLARE
 bool ok=false;
#ifndef __SWITCH__
 void* libs[3]{};
#endif
 Api(){
#ifdef __SWITCH__
#define FF_BIND(n) p_##n=&::n;
  J2ME_FF_FUNCTIONS(FF_BIND)
#undef FF_BIND
#else
  libs[0]=dlopen("libavutil.so.59",RTLD_NOW|RTLD_LOCAL);
  libs[1]=dlopen("libavcodec.so.61",RTLD_NOW|RTLD_LOCAL);
  libs[2]=dlopen("libavformat.so.61",RTLD_NOW|RTLD_LOCAL);
  if(!libs[0]||!libs[1]||!libs[2])return;
#define FF_BIND(n) for(void* lib:libs)if(!p_##n)p_##n=reinterpret_cast<decltype(p_##n)>(dlsym(lib,#n)); if(!p_##n)return;
  J2ME_FF_FUNCTIONS(FF_BIND)
#undef FF_BIND
#endif
  ok=(p_avutil_version()>>16)==59&&(p_avcodec_version()>>16)==61&&(p_avformat_version()>>16)==61;
 }
 // Libraries intentionally live until process exit. No user data or player
 // state lives in this object; the singleton never invalidates function pointers.
};
#undef J2ME_FF_FUNCTIONS
inline Api& api(){static Api a;return a;}
inline bool available(){return api().ok;}
inline bool supports(const std::string& mime){
 auto& a=api();if(!a.ok)return false;
 AVCodecID id=AV_CODEC_ID_NONE;
 if(mime=="audio/mpeg")id=AV_CODEC_ID_MP3;
 else if(mime=="audio/amr")id=AV_CODEC_ID_AMR_NB;
 else if(mime=="audio/amr-wb")id=AV_CODEC_ID_AMR_WB;
 else if(mime=="audio/aac")id=AV_CODEC_ID_AAC;
 else if(mime=="audio/ogg")id=AV_CODEC_ID_VORBIS;
 return id!=AV_CODEC_ID_NONE&&a.p_avcodec_find_decoder(id)!=nullptr;
}
struct Counters{uint64_t attempts=0,success=0,rejected=0,decoded_samples=0;};
inline Counters& counters(){static Counters c;return c;}
struct Input {
 const std::vector<uint8_t>& bytes;size_t pos=0;
 std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 bool expired()const{return std::chrono::steady_clock::now()>deadline;}
 static int read(void* opaque,uint8_t* output,int capacity){
  auto& in=*static_cast<Input*>(opaque);if(in.expired())return AVERROR_EXIT;
  if(capacity<=0)return AVERROR(EINVAL);size_t n=std::min(size_t(capacity),in.bytes.size()-in.pos);
  if(!n)return AVERROR_EOF;std::memcpy(output,in.bytes.data()+in.pos,n);in.pos+=n;return int(n);
 }
 static int64_t seek(void* opaque,int64_t offset,int whence){
  auto& in=*static_cast<Input*>(opaque);if(whence==AVSEEK_SIZE)return int64_t(in.bytes.size());
  whence&=~AVSEEK_FORCE;int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?int64_t(in.pos):whence==SEEK_END?int64_t(in.bytes.size()):-1;
  if(base<0||offset < -base||offset>int64_t(in.bytes.size())-base)return AVERROR(EINVAL);
  in.pos=size_t(base+offset);return int64_t(in.pos);
 }
 static int interrupt(void* opaque){return static_cast<Input*>(opaque)->expired()?1:0;}
 static int forbid_open(AVFormatContext*,AVIOContext**,const char*,int,AVDictionary**){return AVERROR(EACCES);}
};
struct Resources {
 Api& a;AVIOContext* io=nullptr;AVFormatContext* format=nullptr;AVCodecContext* codec=nullptr;
 AVPacket* packet=nullptr;AVFrame* frame=nullptr;AVDictionary* options=nullptr;
 ~Resources(){a.p_av_dict_free(&options);a.p_av_frame_free(&frame);a.p_av_packet_free(&packet);a.p_avcodec_free_context(&codec);
  a.p_avformat_close_input(&format);if(io){a.p_av_free(io->buffer);io->buffer=nullptr;a.p_avio_context_free(&io);}}
};
inline bool decode(const std::vector<uint8_t>& bytes,const std::string&,std::vector<int16_t>& pcm,uint32_t& rate,int& channels,std::string& error){
 auto& counts=counters();++counts.attempts;auto& a=api();
 auto fail=[&](const char* why){error=why;pcm.clear();rate=0;channels=0;++counts.rejected;return false;};
 if(!a.ok)return fail("FFmpeg audio backend unavailable (requires ABI 61/61/59)");
 if(bytes.empty()||bytes.size()>8*1024*1024)return fail("compressed audio resource limit");
 Input input{bytes};Resources r{a};
 auto* buffer=static_cast<uint8_t*>(a.p_av_malloc(4096));if(!buffer)return fail("audio IO allocation");
 r.io=a.p_avio_alloc_context(buffer,4096,0,&input,Input::read,nullptr,Input::seek);
 if(!r.io){a.p_av_free(buffer);return fail("audio IO context allocation");}
 r.format=a.p_avformat_alloc_context();if(!r.format)return fail("audio demux allocation");
 r.format->pb=r.io;r.format->flags|=AVFMT_FLAG_CUSTOM_IO;r.format->max_streams=4;
 r.format->io_open=Input::forbid_open;r.format->interrupt_callback={Input::interrupt,&input};
 a.p_av_dict_set(&r.options,"format_whitelist","mp3,amr,wav,aac,ogg",0);
 a.p_av_dict_set(&r.options,"protocol_whitelist","",0);
 a.p_av_dict_set(&r.options,"analyzeduration","1000000",0);
 a.p_av_dict_set(&r.options,"probesize","1048576",0);
 if(a.p_avformat_open_input(&r.format,nullptr,nullptr,&r.options)<0)return fail("unsupported or malformed audio container");
 a.p_av_dict_free(&r.options);
 if(a.p_avformat_find_stream_info(r.format,nullptr)<0)return fail("audio stream information unavailable");
 const AVCodec* decoder=nullptr;int stream=a.p_av_find_best_stream(r.format,AVMEDIA_TYPE_AUDIO,-1,-1,&decoder,0);
 if(stream<0||!decoder)return fail("no supported audio stream");
 const AVCodecParameters* par=r.format->streams[stream]->codecpar;
 if(par->ch_layout.nb_channels<1||par->ch_layout.nb_channels>2||par->sample_rate<1000||par->sample_rate>192000)return fail("audio channels or rate outside supported limits");
 r.codec=a.p_avcodec_alloc_context3(decoder);if(!r.codec)return fail("audio decoder allocation");
 if(a.p_avcodec_parameters_to_context(r.codec,par)<0)return fail("invalid audio codec parameters");
 r.codec->thread_count=1; // deterministic ownership: the VM remains the sole mixer owner
 if(a.p_avcodec_open2(r.codec,decoder,nullptr)<0)return fail("audio codec initialization failed");
 r.packet=a.p_av_packet_alloc();r.frame=a.p_av_frame_alloc();if(!r.packet||!r.frame)return fail("audio packet allocation");
 pcm.clear();rate=uint32_t(par->sample_rate);channels=par->ch_layout.nb_channels;
 constexpr size_t MaxSamples=16*1024*1024; // 32 MiB decoded PCM, charged by Engine
 auto drain=[&](){
  for(;;){int status=a.p_avcodec_receive_frame(r.codec,r.frame);if(status==AVERROR(EAGAIN)||status==AVERROR_EOF)return true;if(status<0)return false;
   AVFrame* f=r.frame;if(input.expired()||f->nb_samples<0||f->nb_samples>1048576||!f->extended_data||f->sample_rate!=int(rate)||f->ch_layout.nb_channels!=channels)return false;
   const auto format=AVSampleFormat(f->format);const int width=a.p_av_get_bytes_per_sample(format);
   const bool planar=a.p_av_sample_fmt_is_planar(format)!=0;const auto packed=a.p_av_get_packed_sample_fmt(format);
   if(width<1||width>8)return false;size_t n=size_t(f->nb_samples)*size_t(channels);
   if(n>MaxSamples-pcm.size())return false;
   const size_t needed=size_t(f->nb_samples)*size_t(planar?1:channels)*size_t(width);
   if(f->linesize[0]<0||needed>size_t(f->linesize[0]))return false;
   for(int c=0;c<(planar?channels:1);++c)if(!f->extended_data[c])return false;
   size_t old=pcm.size(),size=old+n;if(size>pcm.capacity())pcm.reserve(std::min(MaxSamples,std::max(size,std::max<size_t>(4096,pcm.capacity()*2))));pcm.resize(size);
   for(int i=0;i<f->nb_samples;++i)for(int ch=0;ch<channels;++ch){
    const uint8_t* ptr=f->extended_data[planar?ch:0]+(size_t(i)*(planar?1:channels)+(planar?0:ch))*size_t(width);int16_t value=0;
    if(packed==AV_SAMPLE_FMT_S16)std::memcpy(&value,ptr,2);
    else if(packed==AV_SAMPLE_FMT_U8)value=int16_t((int(*ptr)-128)*256);
    else if(packed==AV_SAMPLE_FMT_S32){int32_t v;std::memcpy(&v,ptr,4);value=int16_t(v>>16);}
    else if(packed==AV_SAMPLE_FMT_S64){int64_t v;std::memcpy(&v,ptr,8);value=int16_t(v>>48);}
    else if(packed==AV_SAMPLE_FMT_FLT||packed==AV_SAMPLE_FMT_DBL){double v;if(packed==AV_SAMPLE_FMT_FLT){float q;std::memcpy(&q,ptr,4);v=q;}else std::memcpy(&v,ptr,8);
     if(!std::isfinite(v))v=0;v=std::max(-1.0,std::min(32767.0/32768.0,v));value=int16_t(std::lrint(v*32768.0));}
    else return false;
    pcm[old+size_t(i)*channels+ch]=value;
   }
   a.p_av_frame_unref(f);
  }
 };
 int status=0;size_t packets=0;
 while((status=a.p_av_read_frame(r.format,r.packet))>=0){
  if(input.expired()||++packets>100000)return fail("audio decoding work limit");
  if(r.packet->stream_index==stream){
   int sent=a.p_avcodec_send_packet(r.codec,r.packet);if(sent==AVERROR(EAGAIN)){if(!drain())return fail("audio frame or output limit");sent=a.p_avcodec_send_packet(r.codec,r.packet);}
   if(sent<0||!drain())return fail("invalid audio packet or decoded PCM limit");
  }
  a.p_av_packet_unref(r.packet);
 }
 if(status!=AVERROR_EOF)return fail("truncated or interrupted audio input");
 if(a.p_avcodec_send_packet(r.codec,nullptr)<0||!drain())return fail("audio final frame failed");
 if(pcm.empty())return fail("empty audio stream");++counts.success;counts.decoded_samples+=pcm.size();return true;
}
}
