#pragma once
#include <cstdint>
#include <deque>
#include <string>

// Observed Happy Farm builds publish their Canvas before its worker has
// initialized the arrays read by paint/keyPressed. This opt-in startup policy
// affects these exact JAR identities only, and ends at resource readiness.
struct FarmStartup {
    enum class State { Off, Waiting, Ready, Failed };
    enum class Profile { None, Farm, GTA, Journey, Alps, Demon };
    Profile profile = Profile::None;
    const char* canvas_class() const {
        switch(profile) { case Profile::Farm:return "f";case Profile::GTA:return "d";
        case Profile::Journey:return "o";case Profile::Alps:return "AlpsFarmCanvas";
        case Profile::Demon:return "c";default:return ""; }
    }
    static Profile identify(const std::string& identity) {
        if (identity=="c98ba9b95789844670a6f9f718dc674ee2cfcadfe77d7541a87137b9e5a36ef0") return Profile::GTA;
        if (identity=="89268f4e52e3713c11ba6315265ee09e431b030d3b7433da503b5c754e07c693") return Profile::Journey;
        if (identity=="a64780fabfaffc3acbf676ca2d27e1419d8338f6b8b20c45212e52b9987aa92b") return Profile::Alps;
        if (identity=="a61b8a462ad573870c9e111c3b872b9187358834e8506d88ad4bb6f3cf4e43d0") return Profile::Demon;
        return selected(identity)?Profile::Farm:Profile::None;
    }
    struct Key { int code; bool release; };
    State state = State::Off;
    int64_t deadline = 0;
    int pending_show = 0; // current Canvas is already shown; callback only is deferred
    int64_t paused_at = -1;
    std::deque<Key> keys;
    static bool selected(const std::string& identity) {
        return identity == "b3700e3a88c48d35ed925788140714a47ea6a10375d7efd51ff71f623e037b0a" ||
               identity == "251e9f36de9bac05c4fa63003b107cdbc43f5f4ae3bf6b5815e3c09af26d8e66" ||
               identity == "bff8e05c4b2d5fbb4e3d04b12b4e8ef424751be75e28e5b230b6ed8a91c4fc80";
    }
    void start(const std::string& identity, int64_t now) {
        *this = FarmStartup{};
        profile=identify(identity);
        if (profile!=Profile::None) { state = State::Waiting; deadline = now + 30000; }
    }
    bool waiting() const { return state == State::Waiting; }
    bool paused() const { return paused_at >= 0; }
    void pause(int64_t now) {
        if (!waiting() || paused()) return;
        paused_at = now;
        keys.clear();
    }
    void resume(int64_t now) {
        if (!waiting() || !paused()) return;
        if (now > paused_at) deadline += now - paused_at;
        paused_at = -1;
    }
    bool update(int64_t now, bool ready) {
        if (!waiting() || paused()) return false;
        if (ready) { state = State::Ready; return true; }
        if (now >= deadline) fail();
        return false;
    }
    bool enqueue(int code, bool release) {
        if (!waiting() || paused()) return false;
        if (keys.size() == 64) { fail(); return false; }
        keys.push_back({code, release}); return true;
    }
    void fail() { state = State::Failed; paused_at = -1; keys.clear(); }
};
