#include "../src/audio_engine.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static void push16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v));
}
static void push32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(uint8_t(v >> 24));
    b.push_back(uint8_t(v >> 16));
    b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v));
}

static std::vector<uint8_t> tiny_midi() {
    std::vector<uint8_t> track = {
        0x00, 0x90, 0x3c, 0x40,
        0x60, 0x80, 0x3c, 0x00,
        0x00, 0xff, 0x2f, 0x00
    };
    std::vector<uint8_t> b;
    b.insert(b.end(), {'M', 'T', 'h', 'd'});
    push32(b, 6);
    push16(b, 0);
    push16(b, 1);
    push16(b, 96);
    b.insert(b.end(), {'M', 'T', 'r', 'k'});
    push32(b, uint32_t(track.size()));
    b.insert(b.end(), track.begin(), track.end());
    return b;
}

int main() {
    j2me_audio::Engine engine;
    std::string error;
    const int handle = engine.load(tiny_midi(), "audio/midi", error);
    assert(handle > 0);
    assert(engine.loop(handle, -1));
    assert(engine.volume(handle, 40) == 40);
    assert(engine.start(handle));
    const int64_t at = engine.seek(handle, 100000);
    assert(at >= 0);
    auto saved = engine.capture();
    assert(saved.size() == 1 && saved[0].handle == handle && saved[0].state == j2me_audio::Engine::Started);
    assert(saved[0].loops == -1 && saved[0].volume == 40 && saved[0].midi);
    engine.reset();
    assert(engine.loadedPlayers() == 0);
    assert(engine.restore(saved) == 1);
    assert(engine.loadedPlayers() == 1);
    assert(engine.activePlayers() == 1);
    assert(engine.state(handle) == j2me_audio::Engine::Started);
    assert(engine.volume(handle) == 40);
    assert(engine.time(handle) == at);
    int16_t pcm[64];
    engine.render(pcm, 16);
    assert(engine.state(handle) == j2me_audio::Engine::Started);
    assert(engine.activePlayers() == 1);
    assert(engine.time(handle) > at);
    std::cout << "PASS audio restore handle=" << handle << " media_us=" << engine.time(handle) << "\n";
    return 0;
}
