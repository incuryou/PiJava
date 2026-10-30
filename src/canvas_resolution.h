#pragma once
#include "logical_screen.h"
#include <unordered_map>
// Resolve the selected JAR once before Java objects are created. Explicit
// display declarations win; otherwise a bounded WxH filename token is a hint.
// Unknown games retain the default; there are no game-name or SHA profiles.
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
    (void)sha;
    Choice result;bool found=false;
    for(const auto& item:props) {
        std::string key=item.first;for(char& c:key)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        const bool original=(key=="nokia-midlet-original-display-size"||key=="lge-midlet-display-original-size");
        if(!original&&key!="semc-screen-size")continue;
        int w=0,h=0;if(!vendor_size(item.second,w,h))return {};
        if(found&&(result.w!=w||result.h!=h))return {};
        result={w,h,original?"vendor_original":"vendor_semc"};found=true;
    }
    return result;
}
inline Choice filename_size(const std::string& jar) {
    const size_t slash=jar.find_last_of("/\\");
    const std::string name=jar.substr(slash==std::string::npos?0:slash+1);
    const auto digit=[](char c){return c>='0'&&c<='9';};
    const auto word=[&](char c){return digit(c)||(c>='A'&&c<='Z')||(c>='a'&&c<='z');};
    Choice result;bool found=false;
    for(size_t i=0;i<name.size();++i){
        if(!digit(name[i])||(i&&word(name[i-1])))continue;
        size_t middle=i;while(middle<name.size()&&digit(name[middle]))++middle;
        if(middle==name.size()||(name[middle]!='x'&&name[middle]!='X'))continue;
        size_t end=middle+1;while(end<name.size()&&digit(name[end]))++end;
        if(end==middle+1||(end<name.size()&&word(name[end])))continue;
        int w=0,h=0;
        if(!logical_screen::parse(name.substr(i,middle-i)+"x"+name.substr(middle+1,end-middle-1),w,h))continue;
        if(found&&(w!=result.w||h!=result.h))return {};
        result={w,h,"filename_explicit"};found=true;i=end-1;
    }
    return result;
}
inline Choice cold_launch(const std::string& jar,const std::unordered_map<std::string,std::string>& props) {
    for(const auto& item:props){
        std::string key=item.first;for(char& c:key)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        // A malformed or conflicting declaration blocks filename fallback.
        if((key=="nokia-midlet-original-display-size"||key=="lge-midlet-display-original-size")||key=="semc-screen-size")return automatic("",props);
    }
    return filename_size(jar);
}
}
