#include <cassert>
#include <fstream>
#include <filesystem>
#include "../src/menu_actions.h"
int main(int argc,char** argv) {
    assert(argc==2);
    using namespace menu_actions;
    assert(move(0,-1,5)==4 && move(4,1,5)==0);
    assert(move(0,1,0)==0 && move(0,-1,1)==0);
    Repeat r;
    assert(r.update(-1,0)==-1);
    assert(r.update(-1,299)==0 && !r.browsing);
    assert(r.update(-1,300)==-1 && r.browsing);
    assert(r.update(-1,379)==0 && r.update(-1,380)==-1);
    assert(r.update(0,381)==0 && !r.browsing);
    assert(r.update(1,382)==1 && r.update(-1,383)==-1);
    r.reset();assert(r.update(-1,500)==-1);
    Deletion d;
    assert(!d.begin("",""));
    assert(d.begin("fixture.jar","测试游戏"));
    assert(d.act(Action::Confirm)==Result::None && d.confirming);
    assert(d.act(Action::Down)==Result::None && d.selection==0);
    assert(d.act(Action::Back)==Result::None && d.open && !d.confirming);
    assert(d.act(Action::Down)==Result::None && d.selection==1);
    assert(d.act(Action::Confirm)==Result::None && d.confirming);
    assert(d.act(Action::Confirm)==Result::DeleteInstant && !d.open);
    d.begin("fixture.jar","测试游戏");d.act(Action::Confirm);
    assert(d.act(Action::Confirm)==Result::DeleteGame && !d.open);
    d.begin("fixture.jar","测试游戏");d.act(Action::Back);assert(!d.open);
    const auto root=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(root/"games");
    auto put=[&](const char* path){std::ofstream(root/path)<<"fixture";};
    put("games/test.jar");put("games/test.jad");put("test.jar");put("save.sav");
    assert(remove_game((root/"games/test.jar").string(),root.string()));
    assert(!std::filesystem::exists(root/"games/test.jar"));
    assert(std::filesystem::exists(root/"test.jar") && std::filesystem::exists(root/"save.sav") && std::filesystem::exists(root/"games/test.jad"));
    assert(!remove_game((root/"save.sav").string(),root.string()));
    assert(!remove_game((root/"games/../test.jar").string(),root.string()));
    std::filesystem::create_directory(root/"fake.jar");
    assert(!remove_game((root/"fake.jar").string(),root.string()));
    std::filesystem::create_symlink(root/"test.jar",root/"link.jar");
    assert(!remove_game((root/"link.jar").string(),root.string()));
    assert(std::filesystem::exists(root/"test.jar"));
    assert(remove_game((root/"test.jar").string(),root.string()));
    assert(!remove_game((root/"test.jar").string(),root.string()));
    std::puts("PASS MenuActionsChecks: wraparound, repeat timing/release/reversal, explicit confirmation/cancel, selected-JAR-only removal, non-JAR/directory/symlink/traversal rejection");
}
