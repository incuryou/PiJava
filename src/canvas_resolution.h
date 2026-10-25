#pragma once
#include "logical_screen.h"
#include <unordered_map>
// Resolve only the selected, already loaded JAR. No directory/JAR scans or
// filename/image-size guesses. Unknown games retain the established default.
namespace canvas_resolution {
struct Choice { int w=240,h=320;const char* source="default"; };
inline bool vendor_size(std::string text,int& w,int& h) {
    if(text.size()>32)return false;
    const size_t comma=text.find(',');
    if(comma==std::string::npos||text.find(',',comma+1)!=std::string::npos)return false;
    auto trim=[](std::string s){
        const auto first=s.find_first_not_of(" \t\r\n");
        if(first==std::string::npos)return std::string{};
        return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
    };
    return logical_screen::parse(trim(text.substr(0,comma))+"x"+trim(text.substr(comma+1)),w,h);
}
inline Choice automatic(const std::string& sha,const std::unordered_map<std::string,std::string>& props) {
    // Verified across startup, menus and gameplay; the handset/file name is
    // not evidence of the game's logical viewport. Overrides stay external.
    if(sha=="e808c5b9878639757edcf67d330536a7b95f553790c2cf23c7d267df06ecb6b8")return {360,360,"verified_jar"};
    Choice result;bool found=false;
    for(const auto& item:props) {
        std::string key=item.first;for(char& c:key)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        const bool original=key=="nokia-midlet-original-display-size";
        if(!original&&key!="semc-screen-size")continue;
        int w=0,h=0;if(!vendor_size(item.second,w,h))return {};
        if(found&&(result.w!=w||result.h!=h))return {};
        result={w,h,original?"vendor_original":"vendor_semc"};found=true;
    }
    return result;
}
}
