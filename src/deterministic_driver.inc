// One deterministic driver for both frontends. Physical presentation and audio
// are passive observers and cannot call the production post-present scheduler.
static bool r4_configure(const std::string& profile) {
    if(profile.empty())return true;
    std::string error;if(!r4::load(profile,error)){std::fprintf(stderr,"R4 %s\n",error.c_str());return false;}
    return true;
}
static bool r4_pause(bool paused) {
    if(!r4::state.active||VMS_EXEC(g_run_depth)||VMS_EXEC(g_pump_depth))return false;
    r4::state.paused=paused;return true;
}
static bool r4_stop() {
    auto& s=r4::state;
    if(!s.active||s.finished||VMS_EXEC(g_run_depth)||VMS_EXEC(g_pump_depth))return false;
    s.remaining=1000000;s.worker_phase=false;release_game_keys();lifecycle_destroy();p7audio::fence();p7::pool().drain();
    r4::finish("STOPPED","HARNESS_STOP");return true;
}
static bool r4_tick() {
    auto& s=r4::state;
    if(!s.active||s.finished||s.paused||s.tick>=s.cfg.steps)return false;
    s.begin_tick();
    // A reserved half-budget keeps synchronous UI callbacks bounded without
    // starving them after a busy worker uses its entire execution quantum.
    // Nested calls consume the same owner, never allocate new tick fuel.
    VMS_EXEC(g_completed_paint241).reset();
    VMS_EXEC(g_speed_frame_open)=false;VMS_EXEC(g_speed_serial_serviced)=false;
    while(s.cursor<s.tape.size()&&s.tape[s.cursor].tick==s.tick){
        const auto e=s.tape[s.cursor++];
        if(!e.release)VMS_PLAT(g_pressed_keys).insert(e.key);else VMS_PLAT(g_pressed_keys).erase(e.key);
        dispatch_key(e.key,e.release);
        if(!s.write("{\"phase\":\"input\",\"tick\":"+std::to_string(s.tick)+",\"seq\":"+std::to_string(e.seq)+",\"key\":"+std::to_string(e.key)+",\"up\":"+(e.release?"true":"false")+"}\n"))r4_abort("OUTPUT_IO");
    }
    refresh_thread(VMS_EXEC(g_main_thread));for(auto& t:VMS_EXEC(g_threads))refresh_thread(t);
    host_events();s.worker_phase=true;thread_resume();s.worker_phase=false;
    host_events();
    if(VMS_RS(g_repaint)){VMS_RS(g_repaint)=false;repaint_current();}
    p7audio::fence();
    const uint64_t end=uint64_t((s.tick+1)*s.cfg.step_us)*48000/1000000;
    uint64_t left=end-s.audio_frames;
    while(left){
        const size_t n=size_t(std::min<uint64_t>(left,1024));int16_t pcm[2048];uint8_t le[4096];
        if(p7audio::g_enabled){p7audio::submit([&]{VMS_PLAT(g_audio).render(pcm,n);});p7audio::fence();}
        else VMS_PLAT(g_audio).render(pcm,n);
        for(size_t i=0;i<n*2;++i){const uint16_t v=uint16_t(pcm[i]);le[2*i]=uint8_t(v);le[2*i+1]=uint8_t(v>>8);}
        if(s.pcm&&std::fwrite(le,1,n*4,s.pcm)!=n*4)r4_abort("OUTPUT_IO");
        s.pcm_hash.update(le,n*4);left-=n;s.audio_frames+=n;
    }
    ++s.tick;
    const uint64_t captures=uint64_t(s.us())*uint64_t(s.cfg.capture_hz)/1000000;
    std::string rgb;
    if(captures>s.captures){
        // Copy a stable CPU buffer; no writable image borrow, Java call, or
        // framebuffer fence can schedule game work here.
        const std::vector<uint8_t> owned=VMS_RS(g_fb);
        rgb=rms_jar_identity(owned.data(),owned.size());s.captures=captures;
        if(captures==1&&s.cfg.payload=="owned-fixtures-only"){
            FILE* f=std::fopen((s.dir+"/capture-0001.rgb").c_str(),"wb");if(!f)r4_abort("OUTPUT_IO");
            bool ok=std::fwrite(owned.data(),1,owned.size(),f)==owned.size();ok=std::fclose(f)==0&&ok;if(!ok)r4_abort("OUTPUT_IO");
        }
    }
    const std::string row="{\"phase\":\"commit\",\"tick\":"+std::to_string(s.tick)+",\"us\":"+std::to_string(s.us())+",\"audio_frames\":"+std::to_string(s.audio_frames)+",\"captures\":"+std::to_string(s.captures)+",\"paints\":"+std::to_string(s.paints)+",\"opcodes\":"+std::to_string(s.opcodes)+",\"rgb_sha256\":\""+rgb+"\"}\n";
    if(!s.write(row))r4_abort("OUTPUT_IO");
    return true;
}
static void r4_run_loop() {
    auto& s=r4::state;
    const std::string flags="{\"phase\":\"identity\",\"version\":\""+std::string(kVer)+"\",\"jar_sha256\":\""+VMS_PLAT(g_rms_suite)+"\",\"jit_available\":"+std::to_string(VMS_NATIVE(g_native_jit).enabled)+",\"fast_compiled\":"+std::to_string(J2ME_OPT_FAST_DISPATCH)+",\"profile\":\""+s.cfg.profile+"\",\"shortcut_policy\":\"bounded_cost_else_java\",\"worker_quota\":"+std::to_string(s.cfg.fuel_per_tick/2)+",\"fixture_seed_hex\":\""+s.cfg.seed+"\"}\n";
    if(!s.write(flags))r4_abort("OUTPUT_IO");
    while(r4_tick()){}
    if(s.paused||s.tick!=s.cfg.steps)r4_abort("INCOMPLETE_DRIVER");
    s.remaining=1000000;s.worker_left=0;s.worker_phase=false;
    release_game_keys();lifecycle_destroy();p7audio::fence();p7::pool().drain();
    r4::finish("COMPLETE","STEP_LIMIT");
}
