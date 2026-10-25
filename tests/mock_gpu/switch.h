#pragma once
struct NWindow {bool valid=true;};
inline NWindow* nwindowGetDefault(){static NWindow window;return &window;}
inline bool nwindowIsValid(NWindow* p){return p&&p->valid;}
