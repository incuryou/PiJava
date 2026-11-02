// The OS applet owns its touchscreen/IME; no game pointer capability is implied.
static int g_text_input_max=0,g_text_input_constraints=0;
static bool g_text_input_single_bmp_name=false;
static struct PendingGameText {
    bool accepted=false;
    game_text::Target target;
    std::string value;
} g_pending_game_text;
static void reset_game_text_input(){g_pending_game_text=PendingGameText{};}
static SwkbdTextCheckResult check_game_text(char* text,size_t size) {
    host_text_input::Config config;config.max_size=g_text_input_max;config.constraints=g_text_input_constraints;
    config.single_bmp_name=g_text_input_single_bmp_name;
    if(host_text_input::valid_config(text,config))return SwkbdTextCheckResult_OK;
    std::snprintf(text,size,"文字不符合此输入框的长度或类型要求，请修改后重试。");
    return SwkbdTextCheckResult_Bad;
}
static void apply_pending_game_text() {
    if(!g_pending_game_text.accepted||VMS_PLAT(g_host_paused)||appletGetFocusState()!=AppletFocusState_InFocus)return;
    // Consume before re-entering Java: user callbacks may replace/close the view.
    auto pending=std::move(g_pending_game_text);reset_game_text_input();
    const auto result=game_text::commit(pending.target,pending.value);
    log_line("TEXTINPUT result="+std::to_string(int(result))); // Never log text/passwords.
    VMS_PLAT(g_cpu_cleared)=0;VMS_PLAT(g_fps_reset)=true;VMS_RS(g_fb_dirty)=true;
}
static bool try_game_text_input(u64 down,u64 held,bool& focus_paused) {
    if(!(down&~g_keymap_blocked&HidNpadButton_A)||(down&HidNpadButton_B)||VMS_PLAT(g_host_paused)||VMS_PLAT(g_exit_sheet)||g_keymap_editor.open)return false;
    game_text::Target before,target;
    if(!game_text::capture(before))return false;
    host_freeze236(true);
    // Releasing held game keys can invoke Java and change the visible screen.
    if(!VMS_PLAT(g_host_frozen236)||!game_text::capture(target)||
       !(before.screen==target.screen)||!(before.field==target.field)||
       before.canvas_slot!=target.canvas_slot||!(before.canvas_buffer==target.canvas_buffer)||
       !(before.canvas_bytes==target.canvas_bytes)) {
        if(VMS_PLAT(g_host_frozen236)&&!focus_paused)host_freeze236(false);
        g_keymap_tracker.clear();g_keymap_blocked|=held;return true;
    }
    SwkbdConfig keyboard{};
    const Result created=swkbdCreate(&keyboard,0);
    if(R_FAILED(created)) {
        if(appletGetFocusState()!=AppletFocusState_InFocus){focus_paused=true;return true;}
        if(!focus_paused)host_freeze236(false);
        log_line("TEXTINPUT unavailable=1 fallback=keypad");return false;
    }
    swkbdConfigMakePresetDefault(&keyboard);
    const auto& config=target.config;
    const int mode=config.constraints&0xffff;
    swkbdConfigSetType(&keyboard,(mode==2||mode==5)?SwkbdType_NumPad:SwkbdType_All);
    if(mode==2||mode==5)swkbdConfigSetLeftOptionalSymbolKey(&keyboard,"-");
    if(mode==5)swkbdConfigSetRightOptionalSymbolKey(&keyboard,".");
    swkbdConfigSetReturnButtonFlag(&keyboard,config.multiline&&mode!=2&&mode!=3&&mode!=5);
    swkbdConfigSetPasswordFlag(&keyboard,(config.constraints&0x10000)?1:0);
    if(config.constraints&(0x10000|0x40000|0x80000))swkbdConfigSetDicFlag(&keyboard,0);
    swkbdConfigSetHeaderText(&keyboard,config.title.c_str());
    swkbdConfigSetGuideText(&keyboard,"确认后回填游戏；取消保留原文。支持触屏或手柄输入。");
    swkbdConfigSetInitialText(&keyboard,config.initial.c_str());
    g_text_input_max=std::min(config.max_size,host_text_input::keyboard_limit);
    g_text_input_constraints=config.constraints;
    g_text_input_single_bmp_name=config.single_bmp_name;
    swkbdConfigSetStringLenMin(&keyboard,0);swkbdConfigSetStringLenMax(&keyboard,g_text_input_max);
    swkbdConfigSetTextCheckCallback(&keyboard,check_game_text);
    g_keymap_tracker.clear();
    log_line("TEXTINPUT open schema=1 kind="+std::string(target.canvas_slot>=0?"canvas_name":"midp")+
             " keyboard="+std::to_string(int((mode==2||mode==5)?SwkbdType_NumPad:SwkbdType_All))+" max="+std::to_string(g_text_input_max));
    char answer[4096]{};
    const Result result=swkbdShow(&keyboard,answer,sizeof answer);
    swkbdClose(&keyboard);g_text_input_max=g_text_input_constraints=0;
    g_text_input_single_bmp_name=false;
    g_keymap_tracker.clear();
    g_keymap_blocked=~u64(0); // Applet dismissal keys must be released first.
    if(R_SUCCEEDED(result)) {
        g_pending_game_text.accepted=true;g_pending_game_text.target=std::move(target);
        g_pending_game_text.value=answer;
    } else log_line("TEXTINPUT cancelled=1");
    focus_paused=appletGetFocusState()!=AppletFocusState_InFocus;
    if(!focus_paused)host_freeze236(false);
    apply_pending_game_text();
    VMS_PLAT(g_cpu_cleared)=0;VMS_PLAT(g_fps_reset)=true;VMS_RS(g_fb_dirty)=true;
    return true;
}
