#include "../src/audio_ffmpeg236.h"
#include "../src/audio_engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){
 if(!audio_ffmpeg236::available()){std::cerr<<"FFmpeg ABI unavailable\n";return 77;}
 j2me_audio::Engine e;e.setDecoder(audio_ffmpeg236::decode);
 for(int i=1;i<argc;++i){std::ifstream in(argv[i],std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(in)),{});std::string error;int h=e.load(b,"audio/auto",error);
  if(!h){std::cerr<<"REJECT "<<argv[i]<<" "<<error<<"\n";return 1;}assert(e.start(h));int16_t pcm[2048];e.render(pcm,1024);auto save=e.capture();j2me_audio::Engine r;assert(r.restore(save)==1);assert(r.time(h)==e.time(h));
  int16_t restored[2048];e.render(pcm,1024);r.render(restored,1024);assert(std::memcmp(pcm,restored,sizeof(pcm))==0);
  std::cout<<"PASS "<<argv[i]<<" duration_us="<<e.duration(h)<<" decoded_samples="<<save[0].pcm.size()<<" snapshot_next_PCM_equal=1\n";e.close(h);
 }
 std::vector<int16_t>pcm;uint32_t rate=0;int ch=0;std::string error;
 assert(!audio_ffmpeg236::decode({1,2,3,4},"audio/auto",pcm,rate,ch,error));
 std::cout<<"PASS codec_invalid_rejected ABI=61/61/59 no_network=1\n";
}
