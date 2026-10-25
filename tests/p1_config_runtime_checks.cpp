// Real BOOT configuration resolver integration; no physical Switch assertions.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iterator>
#include <unistd.h>

static_assert(J2ME_OPT_M3G_TEXTURE238 == 1, "P1 requires native texture support");
static_assert(J2ME_OPT_M3G_SPAN239 == 1, "P1 requires native span support");
static_assert(J2ME_OPT_M3G_ROW240 == 1, "P1 requires native row support");

static unsigned checks = 0;
static const char* legacy_keys[] = {
    "J2ME_M3G_REFERENCE", "J2ME_M3G_SPAN_REFERENCE", "J2ME_M3G_ROW_REFERENCE",
    "J2ME_M3G_STAGE241_REFERENCE", "J2ME_PAINT_HANDOFF_REFERENCE", "J2ME_FLOAT_REFERENCE",
    "J2ME_M3G_SPRITE_REFERENCE", "J2ME_M3G_PREP_REFERENCE", "J2ME_M3G_STAGE242_REFERENCE",
    "J2ME_JIT", "J2ME_PROFILE"
};
static void require(bool ok, const std::string& reason) {
    ++checks;
    if (!ok) { std::cerr << "FAIL P1ConfigRuntimeChecks " << reason << '\n'; std::exit(1); }
}
static void clear_environment() {
    for (const char* key : legacy_keys) require(unsetenv(key) == 0, "clear legacy environment");
    require(unsetenv("J2ME_P1_PROFILE") == 0, "clear profile environment");
}
static void write_bytes(const std::string& path, const std::string& record) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << record;
    output.close();
    require(!output.fail(), "write config fixture");
}
static std::string boot(const std::string& log_path, const std::string* profile_path = nullptr) {
    if (profile_path) require(setenv("J2ME_P1_PROFILE", profile_path->c_str(), 1) == 0, "select profile path");
    else require(unsetenv("J2ME_P1_PROFILE") == 0, "no requested P1 profile");
    // configure_runtime_diagnostics is called once per production process.
    // Reset only process initial diagnostic default before simulating a BOOT.
    VMS_DIAG(g_p1).enabled = true;
    VMS_PLAT(g_log).clear();
    VMS_PLAT(g_log).open(log_path, std::ios::trunc);
    require(VMS_PLAT(g_log).is_open(), "open real production log sink");
    configure_runtime_diagnostics();
    VMS_PLAT(g_log).flush();
    VMS_PLAT(g_log).close();
    std::ifstream input(log_path);
    require(input.good(), "read captured BOOT log");
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
static void contains(const std::string& log, const std::string& text) {
    require(log.find(text) != std::string::npos, "BOOT evidence includes: " + text);
}
static void effective(const p1::Profile& expected) {
    require(VMS_DIAG(g_p1_profile).active && VMS_DIAG(g_p1_profile).id == expected.id && VMS_DIAG(g_p1_profile).error.empty(), "active profile identity");
    require(VMS_RS(g_m3g_mesh242_enabled) == expected.geometry_native, "effective geometry");
    require(VMS_RS(g_host_boundary242_enabled) == expected.host_boundary, "effective host boundary independent of geometry");
    require(VMS_EXEC(g_completed_paint241).enabled == expected.completed_handoff, "effective completed handoff independent of boundary");
    require(VMS_RS(g_m3g_sprite241_enabled) == expected.sprite_native && VMS_RS(g_m3g_prep241_enabled) == expected.prep_native &&
            VMS_EXEC(g_fast_float241_enabled) == expected.float_batch, "effective auxiliary switches");
    require(VMS_DIAG(g_p1).enabled == expected.diagnostics, "effective bounded diagnostics switch");
    require(VMS_RS(g_m3g_texture238_enabled) && VMS_RS(g_m3g_span239_enabled) && VMS_RS(g_m3g_row240_enabled), "fixed texture span row all one");
    require(VMS_EXEC(g_speed_cap) == 0 && speed_period_us() == 0, "fixed speed native");
    require(!VMS_NATIVE(g_native_jit).enabled && !VMS_EXEC(g_execution_profile).enabled, "JIT and heavy method profiler disabled");
    require(host_boundary242_active() == expected.host_boundary, "production boundary helper reads independently resolved flag");
}
static void field_sources(const std::string& log, const p1::Profile& expected) {
    const char* names[] = {"geometry_native", "host_boundary", "completed_handoff", "sprite_native",
                          "prep_native", "float_batch", "diagnostics"};
    const bool values[] = {expected.geometry_native, expected.host_boundary, expected.completed_handoff,
                          expected.sprite_native, expected.prep_native, expected.float_batch, expected.diagnostics};
    for (unsigned field = 0; field < 7; ++field) {
        const std::string value = std::to_string(values[field]);
        contains(log, "P1CONFIG field=" + std::string(names[field]) + " requested=" + value + " effective=" +
            value + " source=" + ((expected.override_mask & (1u << field)) ? "explicit_config" : "builtin_profile"));
    }
    contains(log, "P1CONFIG fixed texture_native=1 span_native=1 row_native=1 jit_requested=0 renderer=cpu speed=native source=profile_fixed");
}
static void legacy(bool reference, int preserved_speed) {
    require(!VMS_DIAG(g_p1_profile).active, "legacy profile inactive");
    require(VMS_RS(g_m3g_texture238_enabled) == !reference && VMS_RS(g_m3g_span239_enabled) == !reference &&
            VMS_RS(g_m3g_row240_enabled) == !reference, "legacy texture span row configuration retained");
    require(VMS_RS(g_m3g_mesh242_enabled) == !reference && VMS_RS(g_host_boundary242_enabled) == !reference,
            "legacy geometry and its original coupled boundary retained");
    require(VMS_EXEC(g_completed_paint241).enabled == !reference && VMS_RS(g_m3g_sprite241_enabled) == !reference &&
            VMS_RS(g_m3g_prep241_enabled) == !reference && VMS_EXEC(g_fast_float241_enabled) == !reference,
            "legacy stage241 flags retained");
    require(VMS_EXEC(g_speed_cap) == preserved_speed && VMS_DIAG(g_p1).enabled, "missing/invalid P1 does not impose P1 speed or diagnostics settings");
}

int main(int argc, char** argv) {
    require(argc == 2, "profile directory argument required");
    std::cout << std::unitbuf;
    clear_environment();
    char pattern[] = "/tmp/j2me-p1-config-runtime-XXXXXX";
    char* directory = mkdtemp(pattern);
    require(directory != nullptr, "isolated temporary directory");
    const std::string config = std::string(directory) + "/p1-profile.cfg";
    const std::string log_path = std::string(directory) + "/boot.log";
    for (const char* id : {"P1-R", "P1-C", "P1-M00", "P1-M01", "P1-M10", "P1-M11"}) {
        const std::string path = std::string(argv[1]) + "/" + id + "/p1-profile.cfg";
        const auto expected = p1::read_profile(path);
        require(expected.active, "shipped builtin fixture valid");
        // Hostile legacy sources must be overridden completely by every profile.
        for (const char* key : legacy_keys) require(setenv(key, "1", 1) == 0, "request conflicting legacy flag");
        VMS_EXEC(g_speed_cap) = 60;
        const auto log = boot(log_path, &path);
        effective(expected);
        field_sources(log, expected);
        for (const char* key : legacy_keys)
            contains(log, "ignored_source=" + std::string(key) + " reason=explicit_profile");
        contains(log, "P1CONFIG active=1 profile=" + std::string(id));
        contains(log, "speed=native renderer=cpu jit_requested=0");
        contains(log, "JIT mode=opt_in requested=1 enabled=0 source=P1_forced_off");
        std::cout << "PASS runtime builtin=" << id << " ignored_legacy_and_jit=1 fixed_native=1\n";
    }
    clear_environment();
    for (int geometry = 0; geometry < 2; ++geometry) for (int boundary = 0; boundary < 2; ++boundary)
        for (int handoff = 0; handoff < 2; ++handoff) {
            const std::string record = "profile=P1-R\ngeometry_native=" + std::to_string(geometry) +
                "\nhost_boundary=" + std::to_string(boundary) + "\ncompleted_handoff=" + std::to_string(handoff) +
                "\nsprite_native=1\nprep_native=1\nfloat_batch=1\ndiagnostics=0\n";
            write_bytes(config, record);
            const auto log = boot(log_path, &config);
            const auto expected = p1::parse_profile(record);
            effective(expected);
            field_sources(log, expected);
            std::cout << "PASS runtime geometry=" << geometry << " B=" << boundary << " H=" << handoff << " independent=1\n";
        }
    // A second minimal config distinguishes builtin sources from explicit
    // overrides, while deliberately disagreeing on geometry versus boundary.
    const std::string config2 = "profile=P1-M11\ngeometry_native=0\n";
    write_bytes(config, config2);
    auto log = boot(log_path, &config);
    effective(p1::parse_profile(config2));
    field_sources(log, p1::parse_profile(config2));
    contains(log, "explicit_mask=1");
    std::cout << "PASS runtime config2_requested_effective_source=1 geometry_boundary_independent=1\n";
    clear_environment();
    VMS_EXEC(g_speed_cap) = 30;
    log = boot(log_path);
    legacy(false, 30);
    require(VMS_DIAG(g_p1_profile).error.empty(), "unset profile is not rejected");
    contains(log, "P1CONFIG active=0 source=legacy defaults_preserved=1");
    require(log.find("P1CONFIG rejected") == std::string::npos, "legacy no-profile BOOT has no false rejection");
    std::cout << "PASS runtime unset_profile_preserves_legacy=1\n";
    const std::string missing = std::string(directory) + "/missing.cfg";
    log = boot(log_path, &missing);
    legacy(false, 30);
    require(VMS_DIAG(g_p1_profile).error.empty(), "missing profile has no parse error");
    std::cout << "PASS runtime missing_profile_preserves_legacy=1\n";
    for (const char* key : legacy_keys) require(setenv(key, "1", 1) == 0, "activate legacy reference mode");
    log = boot(log_path, &missing);
    legacy(true, 30);
    require(VMS_EXEC(g_execution_profile).enabled && !VMS_NATIVE(g_native_jit).enabled, "legacy heavy profiler retains precedence over requested JIT");
    std::cout << "PASS runtime missing_profile_retains_legacy_reference_flags=1\n";
    write_bytes(config, "profile=P1-M11\ngeometry_native=1\nhost_boundary=1\ncompleted_handoff=1\nspeed=60\n");
    log = boot(log_path, &config);
    legacy(true, 30);
    require(!VMS_DIAG(g_p1_profile).error.empty() && VMS_DIAG(g_p1_profile).id.empty() && VMS_DIAG(g_p1_profile).override_mask == 0,
            "malformed profile tail rejects full config atomically");
    contains(log, "P1CONFIG rejected source=" + config);
    contains(log, "effective=legacy");
    require(log.find("P1CONFIG active=1") == std::string::npos, "rejected valid prefix never activates");
    std::cout << "PASS runtime invalid_profile_atomic_legacy_fallback=1\n";
    clear_environment();
    require(std::remove(config.c_str()) == 0 && std::remove(log_path.c_str()) == 0 && rmdir(directory) == 0,
            "remove isolated fixtures");
    std::cout << "ALL P1 CONFIG RUNTIME CHECKS COMPLETE " << checks << '\n';
}
