#include "../src/audio_engine.h"
#include <cassert>
#include <iostream>

using Bytes = std::vector<uint8_t>;
static void le16(Bytes& bytes, unsigned value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); }
static void le32(Bytes& bytes, unsigned value) { le16(bytes,value); le16(bytes,value >> 16); }
static Bytes wav() {
    Bytes bytes = {'R','I','F','F'};
    le32(bytes,36+24000); bytes.insert(bytes.end(),{'W','A','V','E','f','m','t',' ',16,0,0,0});
    le16(bytes,1); le16(bytes,1); le32(bytes,48000); le32(bytes,96000); le16(bytes,2); le16(bytes,16);
    bytes.insert(bytes.end(),{'d','a','t','a'}); le32(bytes,24000);
    for (int i=0;i<12000;++i) le16(bytes,1000);
    return bytes;
}
static Bytes midi() {
    return {'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,
        0,0x90,60,100,96,0x80,60,0,0,0xff,47,0};
}
static int load(j2me_audio::Engine& engine, const Bytes& data) {
    std::string error; int handle=engine.load(data,"",error);
    assert(handle>0 && error.empty()); engine.volume(handle,10); assert(engine.start(handle)); return handle;
}
static long long energy(const std::vector<int16_t>& samples) {
    long long value=0; for (int16_t sample:samples) value+=std::abs(int(sample)); return value;
}
int main() {
    // Self-authored PCM/MIDI: compare actual mixed samples with each isolated voice.
    j2me_audio::Engine pcmEngine,midiEngine,toneEngine,mixedEngine;
    int pcm=load(pcmEngine,wav()), music=load(midiEngine,midi());
    int tone=toneEngine.playTone(69,250,10); assert(tone>0);
    int mixedPcm=load(mixedEngine,wav()), mixedMusic=load(mixedEngine,midi());
    int mixedTone=mixedEngine.playTone(69,250,10); assert(mixedTone>0);
    std::vector<int16_t> p(9600),m(9600),t(9600),mix(9600);
    pcmEngine.render(p.data(),4800); midiEngine.render(m.data(),4800);
    toneEngine.render(t.data(),4800); mixedEngine.render(mix.data(),4800);
    assert(energy(p)>0 && energy(m)>0 && energy(t)>0);
    for (size_t i=0;i<mix.size();++i) assert(std::abs(int(mix[i])-int(p[i])-int(m[i])-int(t[i]))<=2);
    assert(mixedEngine.state(mixedPcm)==400 && mixedEngine.state(mixedMusic)==400 && mixedEngine.state(mixedTone)==400);
    assert(mixedEngine.time(mixedPcm)==100000 && mixedEngine.time(mixedMusic)==100000 && mixedEngine.time(mixedTone)==100000);
    assert(mixedEngine.stop(mixedPcm));
    midiEngine.render(m.data(),4800); toneEngine.render(t.data(),4800); mixedEngine.render(mix.data(),4800);
    for (size_t i=0;i<mix.size();++i) assert(std::abs(int(mix[i])-int(m[i])-int(t[i]))<=1);
    assert(mixedEngine.time(mixedPcm)==100000 && mixedEngine.time(mixedMusic)==200000 && mixedEngine.time(mixedTone)==200000);
    j2me_audio::Engine twoPlayers,playerTone,lowTone,highTone,twoTones;
    load(twoPlayers,wav()); load(twoPlayers,midi());
    // Fresh isolated buffers at time zero are independent references for each pair.
    pcmEngine.seek(pcm,0); midiEngine.seek(music,0); toneEngine.seek(tone,0);
    pcmEngine.render(p.data(),4800); midiEngine.render(m.data(),4800); toneEngine.render(t.data(),4800);
    twoPlayers.render(mix.data(),4800);
    for (size_t i=0;i<mix.size();++i) assert(std::abs(int(mix[i])-int(p[i])-int(m[i]))<=1);
    load(playerTone,wav()); assert(playerTone.playTone(69,250,10)>0); playerTone.render(mix.data(),4800);
    for (size_t i=0;i<mix.size();++i) assert(std::abs(int(mix[i])-int(p[i])-int(t[i]))<=1);
    assert(lowTone.playTone(60,250,10)>0 && highTone.playTone(72,250,10)>0);
    assert(twoTones.playTone(60,250,10)>0 && twoTones.playTone(72,250,10)>0);
    lowTone.render(m.data(),4800); highTone.render(t.data(),4800); twoTones.render(mix.data(),4800);
    assert(energy(m)>0 && energy(t)>0);
    for (size_t i=0;i<mix.size();++i) assert(std::abs(int(mix[i])-int(m[i])-int(t[i]))<=1);
    assert(pcmEngine.close(pcm) && midiEngine.close(music));
    std::cout << "mixing compatibility: two Players, Player + tone, two tones, triple sum and independent stop passed\n";
}
