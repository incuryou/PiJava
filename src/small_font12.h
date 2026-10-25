#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
namespace small_font12 {
struct Glyph { uint16_t code; uint8_t bits[24]; };
// Immutable asset cache, no Java handles or mutable VM/snapshot state.
inline const uint8_t* find(int cp) {
    static const std::vector<Glyph> glyphs=[] {
        std::vector<Glyph> out;FILE* f=std::fopen("romfs:/font/small12.bin","rb");
        if(!f)f=std::fopen("romfs/font/small12.bin","rb");if(!f)return out;
        uint8_t header[4];if(std::fread(header,1,4,f)!=4){std::fclose(f);return out;}
        const uint32_t n=uint32_t(header[0])|(uint32_t(header[1])<<8)|(uint32_t(header[2])<<16)|(uint32_t(header[3])<<24);
        if(n>65536){std::fclose(f);return out;}out.reserve(n);
        for(uint32_t i=0;i<n;++i) {
            uint8_t raw[26];if(std::fread(raw,1,26,f)!=26){out.clear();break;}
            Glyph g{};g.code=uint16_t(raw[0])|(uint16_t(raw[1])<<8);
            if(!out.empty()&&g.code<=out.back().code){out.clear();break;}
            std::copy(raw+2,raw+26,g.bits);out.push_back(g);
        }
        if(std::fgetc(f)!=EOF||std::ferror(f))out.clear();std::fclose(f);return out;
    }();
    if(cp<0||cp>65535)return nullptr;
    const auto it=std::lower_bound(glyphs.begin(),glyphs.end(),cp,[](const Glyph& g,int code){return g.code<code;});
    return it!=glyphs.end()&&it->code==cp?it->bits:nullptr;
}
}
