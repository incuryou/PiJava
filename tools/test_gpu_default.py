#!/usr/bin/env python3
"""Run actual host selection/startup/actions; SDK doubles do not measure FPS."""
import re
import subprocess
import build


def function(host, signature):
    start = host.index(signature)
    return host[start:host.index('\n}\n', start) + 3]


def main():
    host = (build.ROOT / 'src/platform/switch_host.inc').read_text()
    out = build.BUILD / 'gpu-default-checks'
    out.mkdir(parents=True, exist_ok=True)
    match = re.search(r'static bool (select_presenter|toggle_presenter)\(\)', host)
    assert match, 'host backend selector missing'
    selector = function(host, match.group(0))
    name = match.group(1)
    common = '''#include <cassert>
#include <string>
#include <vector>
#include "presentation/gpu_filter.h"
using display_filter::Filter;
namespace host_menu {
struct Settings {Filter filter=Filter::Original;};
static Filter saved=Filter::Original;static bool change=true;
static bool load_settings(const char*,Settings& s){s.filter=saved;return true;}
static bool change_filter(Settings& s,int){if(!change)return false;
    s.filter=Filter((int(s.filter)+1)%6);return true;}
}
static host_menu::Settings g_menu_settings;
static bool g_gpu_active=false,init_ok=true,cpu_owned=false,gpu_owned=false;
static int inits=0,closes=0;
struct Profile {bool active=false;};static Profile g_p1_profile;
static std::vector<std::string> logs;
struct Gpu {bool init(){assert(!cpu_owned&&!gpu_owned);++inits;gpu_owned=init_ok;return init_ok;}};
static Gpu g_gpu_present;
#define VMS_PLAT(x) x
#define VMS_DIAG(x) x
static void log_line(const std::string& s){logs.push_back(s);}
static void log_drain(){}
static void close_presenter(){++closes;g_gpu_active=false;cpu_owned=gpu_owned=false;}
static void reset(){g_gpu_active=false;cpu_owned=gpu_owned=false;init_ok=true;
    inits=closes=0;g_p1_profile.active=false;logs.clear();}
'''

    def run(label, body):
        path = out / (label + '.cpp')
        path.write_text(common + selector + '\n' + body)
        for enabled in (False, True):
            exe = out / (label + ('-gpu' if enabled else '-cpu'))
            subprocess.run(['g++', '-std=c++17', '-I' + str(build.ROOT / 'src')]
                           + (['-DJ2ME_ENABLE_DEKO3D'] if enabled else [])
                           + [str(path), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    run('selection', '''int main(){
reset();g_menu_settings.filter=Filter::Original;cpu_owned=true;
''' + name + '''();assert(!g_gpu_active&&inits==0&&"Original must use CPU");
for(Filter f:{Filter::Original}){
    reset();g_menu_settings.filter=f;cpu_owned=true;
    ''' + name + '''();assert(!g_gpu_active&&inits==0);
}
for(Filter f:{Filter::Bilinear,Filter::Scale2x,Filter::Lcd,Filter::Crt,Filter::ScanlinesDX}){
    reset();g_menu_settings.filter=f;cpu_owned=true;
    ''' + name + '''();
#ifdef J2ME_ENABLE_DEKO3D
    assert(g_gpu_active&&inits==1&&closes==1&&!cpu_owned&&gpu_owned);
    ''' + name + '''();assert(g_gpu_active&&inits==1&&closes==1);
    g_menu_settings.filter=Filter::Original;''' + name + '''();
    assert(!g_gpu_active&&!gpu_owned&&closes==2);
    cpu_owned=true;g_menu_settings.filter=f;''' + name + '''();
    assert(g_gpu_active&&inits==2&&closes==3);
    g_p1_profile.active=true;''' + name + '''();assert(!g_gpu_active&&!gpu_owned);
#else
    assert(!g_gpu_active&&inits==0);
#endif
}
reset();init_ok=false;cpu_owned=true;g_menu_settings.filter=Filter::Lcd;
''' + name + '''();assert(!g_gpu_active&&!gpu_owned);
g_menu_settings.filter=Filter::Original;''' + name + '''();assert(!g_gpu_active);
reset();g_p1_profile.active=true;g_menu_settings.filter=Filter::Crt;
''' + name + '''();assert(!g_gpu_active&&inits==0);
}''')

    main = host.index('int main(int argc, char** argv)')
    config_start = host.index('    const bool config_loaded = host_menu::load_settings(', main)
    status_start = host.index('    std::string status;', config_start)
    status_end = host.index('    int sd_jars', status_start)
    config = host[config_start:status_start]
    status = host[status_start:status_end]
    assert 'select_presenter();' in config, 'load saved filter before selecting backend'
    assert host.index('select_presenter();', main) > config_start, 'no unconditional boot GPU'
    assert host.index('select_presenter();', config_start) < host.index('    draw_menu(items, sel, status);', main)
    run('startup', '''int main(){
for(Filter f:{Filter::Original,Filter::Bilinear,Filter::Scale2x,Filter::Lcd,Filter::Crt,Filter::ScanlinesDX}){
for(bool fail:{false,true})for(bool profile:{false,true}){
 reset();host_menu::saved=f;init_ok=!fail;g_p1_profile.active=profile;
''' + config + status + '''
assert(config_loaded&&g_menu_settings.filter==f);
#ifdef J2ME_ENABLE_DEKO3D
const bool wanted=j2me_present::gpu_filter::supported(f)&&!profile;
assert(g_gpu_active==(wanted&&!fail));assert(inits==int(wanted));
if(!j2me_present::gpu_filter::supported(f)||g_gpu_active)assert(status.empty());
else if(profile)assert(status.find("P1")!=std::string::npos);
else assert(status.find("CPU")!=std::string::npos);
#else
assert(!g_gpu_active&&inits==0);
#endif
}}}''')

    change_start = host.index('                    settings_changed=host_menu::change_filter(', main)
    change_end = host.index('                } else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Fps)', change_start)
    change = host[change_start:change_end]
    assert 'select_presenter();' in change, 'actual filter-change action must select backend'
    run('filter-change', '''int main(){
reset();g_menu_settings.filter=Filter::ScanlinesDX;select_presenter();
bool settings_changed=false;host_menu::change=true;
''' + change + '''
assert(settings_changed&&g_menu_settings.filter==Filter::Original&&!g_gpu_active&&!gpu_owned);
g_menu_settings.filter=Filter::Crt;init_ok=false;select_presenter();
const int attempted=inits;host_menu::change=false;
''' + change + '''
assert(!settings_changed&&inits==attempted&&!g_gpu_active);
reset();g_menu_settings.filter=Filter::Original;host_menu::change=true;
for(int step=1;step<=6;++step){
''' + change + '''
assert(settings_changed&&int(g_menu_settings.filter)==step%6);
#ifdef J2ME_ENABLE_DEKO3D
assert(g_gpu_active==(step<6));assert(inits==1);assert(closes==(step<6?1:2));
#else
assert(!g_gpu_active&&inits==0);
#endif
}
}''')
    compose = host[host.index('static void compose_host_surface'):host.index('static void present()')]
    assert 'renderer_badge' not in host, 'normal release must not show GPU test status'
    assert '892,8,376,28' not in compose, 'remove the test HUD background as well'
    assert 'host_menu::render_fps(' in compose, 'retain the original FPS display'
    assert 'toggle_presenter' not in host, 'remove obsolete unconditional toggle'
    print('PASS actual selector, saved-filter startup, filter-change action, P1/failure/CPU-only, backend lifetime, automatic five-filter routing and no test HUD')


if __name__ == '__main__':
    main()
