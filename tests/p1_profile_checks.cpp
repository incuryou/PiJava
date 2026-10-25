#include "../src/p1_profile.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

static int checks = 0;
static void require(bool ok, const char* reason) {
    ++checks;
    if (!ok) { std::cerr << "FAIL P1ProfileChecks " << reason << '\n'; std::exit(1); }
}
static void write_bytes(const std::string& path, const std::string& record) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(record.data(), static_cast<std::streamsize>(record.size()));
    stream.close();
    require(!stream.fail(), "write fixture");
}
static void rejected(const std::string& record) {
    const p1::Profile profile = p1::parse_profile(record);
    require(!profile.active && !profile.error.empty(), "malformed config explicitly rejected");
    require(profile.id.empty() && !profile.geometry_native && !profile.host_boundary &&
            !profile.completed_handoff && !profile.sprite_native && !profile.prep_native &&
            !profile.float_batch && profile.diagnostics && profile.override_mask == 0,
            "rejection has no partially applied fields");
}
static void match(const p1::Profile& profile, const std::string& id,
                  bool geometry, bool boundary, bool handoff, bool auxiliary) {
    require(profile.active && profile.error.empty() && profile.id == id, "accepted profile identity");
    require(profile.geometry_native == geometry && profile.host_boundary == boundary &&
            profile.completed_handoff == handoff && profile.sprite_native == auxiliary &&
            profile.prep_native == auxiliary && profile.float_batch == auxiliary && profile.diagnostics,
            "builtin resolves every field independently of legacy configuration");
}

int main(int argc, char** argv) {
    struct Expected { const char* id; bool geometry, boundary, handoff, auxiliary; };
    const Expected expected[] = {
        {"P1-R", true, true, false, false}, {"P1-C", false, false, true, true},
        {"P1-M00", true, false, false, true}, {"P1-M01", true, false, true, true},
        {"P1-M10", true, true, false, true}, {"P1-M11", true, true, true, true}
    };
    for (const Expected& value : expected) {
        const auto profile = p1::parse_profile(std::string("profile=") + value.id);
        match(profile, value.id, value.geometry, value.boundary, value.handoff, value.auxiliary);
        require(profile.override_mask == 0, "builtin values report builtin source");
        if (argc > 1) {
            const auto shipped = p1::read_profile(std::string(argv[1]) + "/" + value.id + "/p1-profile.cfg");
            match(shipped, value.id, value.geometry, value.boundary, value.handoff, value.auxiliary);
            require(shipped.override_mask == 127, "copy-ready configs explicitly declare all switches");
        }
    }
    // Exercise all B/H/geometry combinations, including values that disagree
    // with each builtin. No switch is allowed to act as a proxy for another.
    for (const Expected& value : expected) for (int geometry = 0; geometry < 2; ++geometry)
        for (int boundary = 0; boundary < 2; ++boundary) for (int handoff = 0; handoff < 2; ++handoff) {
            const auto profile = p1::parse_profile("geometry_native=" + std::to_string(geometry) +
                "\nhost_boundary=" + std::to_string(boundary) + "\ncompleted_handoff=" + std::to_string(handoff) +
                "\nprofile=" + value.id + "\n");
            match(profile, value.id, geometry, boundary, handoff, value.auxiliary);
            require(profile.override_mask == (p1::Geometry | p1::Boundary | p1::Handoff), "override source mask");
        }
    const char* keys[] = {"geometry_native", "host_boundary", "completed_handoff", "sprite_native",
                          "prep_native", "float_batch", "diagnostics"};
    for (const Expected& value : expected) for (unsigned field = 0; field < 7; ++field)
        for (int enabled = 0; enabled < 2; ++enabled) {
            const auto isolated = p1::parse_profile("profile=" + std::string(value.id) + "\n" +
                keys[field] + "=" + std::to_string(enabled) + "\n");
            const bool actual[] = {isolated.geometry_native, isolated.host_boundary, isolated.completed_handoff,
                isolated.sprite_native, isolated.prep_native, isolated.float_batch, isolated.diagnostics};
            const bool baseline[] = {value.geometry, value.boundary, value.handoff,
                value.auxiliary, value.auxiliary, value.auxiliary, true};
            require(isolated.active && isolated.error.empty() && isolated.override_mask == (1u << field),
                    "each individual explicit field accepted");
            for (unsigned other = 0; other < 7; ++other)
                require(actual[other] == (other == field ? enabled != 0 : baseline[other]),
                        "individual override affects only its own field");
        }
    auto profile = p1::parse_profile("# comment\r\n schema_version = 1\r\n profile = P1-R # selected\r\n"
        "sprite_native=true\nprep_native=1\nfloat_batch=true\ndiagnostics=false\n"
        "texture_native=true\nspan_native=1\nrow_native=true\njit_requested=false\nrenderer=cpu\nspeed=native\n");
    require(profile.active && profile.sprite_native && profile.prep_native && profile.float_batch &&
            !profile.diagnostics && profile.geometry_native && profile.host_boundary && !profile.completed_handoff,
            "comments CRLF whitespace and explicit booleans");
    require(profile.override_mask == (p1::Sprite | p1::Prep | p1::Float | p1::Diagnostics), "individual override sources");
    const auto previous = p1::parse_profile("profile=P1-C\ngeometry_native=1\nhost_boundary=1\ndiagnostics=0\n");
    profile = p1::parse_profile("profile=P1-C\n");
    require(previous.active && profile.active && !profile.geometry_native && !profile.host_boundary &&
            profile.diagnostics && profile.override_mask == 0, "new load inherits no preceding profile state");
    std::string limit = "profile=P1-R\n#";
    limit += std::string(p1::max_profile_bytes - limit.size(), 'x');
    require(p1::parse_profile(limit).active, "exactly 4096 bytes accepted");
    rejected(limit + "x");
    for (const std::string& record : {
            std::string(), std::string("# comment only\n"), std::string("geometry_native=1\n"),
            std::string("profile=P1-X\n"), std::string("profile=P1-R\nprofile=P1-C\n"),
            std::string("profile=P1-R\ngeometry_native=0\ngeometry_native=1\n"),
            std::string("profile=P1-R\nunknown=1\n"), std::string("profile=P1-R\ngeometry_native=yes\n"),
            std::string("profile=P1-R\nhost_boundary=2\n"), std::string("profile=P1-R\ncompleted_handoff=-1\n"),
            std::string("profile=P1-R\nprep_native=\n"), std::string("profile=P1-R\n=1\n"),
            std::string("profile=P1-R\ndiagnostics=0=1\n"), std::string("profile P1-R\n"),
            std::string("profile=P1-R\nschema_version=2\n"), std::string("profile=P1-R\nschema_version=01\n"),
            std::string("profile=P1-C\ntexture_native=0\n"), std::string("profile=P1-C\nspan_native=0\n"),
            std::string("profile=P1-C\nrow_native=0\n"), std::string("profile=P1-C\njit_requested=1\n"),
            std::string("profile=P1-C\nrenderer=gpu\n"), std::string("profile=P1-C\nspeed=30\n"),
            std::string("profile=P1-C\nrenderer=cpu\nrenderer=cpu\n"),
            std::string("profile=P1-C\nschema_version=1\nschema_version=1\n"),
            std::string("{\"profile\":\"P1-R\"}"),
            std::string("profile=P1-C\nsprite_native=FALSE\n"), std::string("profile=P1-C\nfloat_batch=off\n")})
        rejected(record);
    rejected(std::string("profile=P1-R\0\n", 15));
    rejected("profile=P1-R\n\x1b");
    rejected("profile=P1-R\n\x7f");
    char pattern[] = "/tmp/j2me-p1-profile-XXXXXX";
    char* directory = mkdtemp(pattern);
    require(directory != nullptr, "isolated temporary directory");
    const std::string path = std::string(directory) + "/p1-profile.cfg";
    profile = p1::read_profile(path);
    require(!profile.active && profile.error.empty(), "missing profile preserves legacy mode without error");
    write_bytes(path, "profile=P1-R\n");
    match(p1::read_profile(path), "P1-R", true, true, false, false);
    profile = p1::read_profile(path + "/p1-profile.cfg");
    require(!profile.active && !profile.error.empty(), "unopenable non-directory path is explicit error");
    write_bytes(path, "profile=P1-C\ngeometry_native=1\nhost_boundary=invalid\n");
    profile = p1::read_profile(path);
    require(!profile.active && !profile.error.empty() && !profile.geometry_native && profile.id.empty(),
            "file read rejects malformed tail atomically");
    write_bytes(path, limit);
    require(p1::read_profile(path).active, "file bound includes exactly 4096 bytes");
    write_bytes(path, limit + "x");
    require(!p1::read_profile(path).error.empty(), "file reads one byte beyond bound and rejects");
    profile = p1::read_profile(directory);
    require(!profile.active && !profile.error.empty(), "directory read is error rather than missing profile");
    profile = p1::read_profile(path + std::string("\0ignored", 8));
    require(!profile.active && !profile.error.empty(), "embedded NUL path explicitly rejected");
    require(!p1::read_profile("").error.empty(), "empty path explicitly rejected");
    require(std::remove(path.c_str()) == 0 && rmdir(directory) == 0, "remove fixtures");
    std::cout << "ALL P1 PROFILE CHECKS COMPLETE " << checks << '\n';
}
