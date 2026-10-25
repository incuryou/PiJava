#pragma once
#include <cstdint>
#include <string>
namespace numeric236 {
// Java floating literals, not the larger libc grammar (e.g. nan(payload), inf).
// No locale changes, whitespace is exactly String.trim's U+0000..U+0020 range.
inline bool literal(std::string& s) {
    size_t begin=0,end=s.size();
    while(begin<end&&static_cast<unsigned char>(s[begin])<=32)++begin;
    while(end>begin&&static_cast<unsigned char>(s[end-1])<=32)--end;
    s=s.substr(begin,end-begin);if(s.empty())return false;
    size_t i=(s[0]=='+'||s[0]=='-')?1:0;
    if(s.substr(i)=="NaN"||s.substr(i)=="Infinity")return true;
    if(i==s.size())return false;
    bool hex=i+2<s.size()&&s[i]=='0'&&(s[i+1]=='x'||s[i+1]=='X');
    if(hex)i+=2;
    auto digit=[&](char c){return (c>='0'&&c<='9')||(hex&&((c>='a'&&c<='f')||(c>='A'&&c<='F')));};
    size_t digits=0;while(i<s.size()&&digit(s[i])){++i;++digits;}
    if(i<s.size()&&s[i]=='.'){++i;while(i<s.size()&&digit(s[i])){++i;++digits;}}
    if(!digits)return false;
    bool exponent=i<s.size()&&(hex?(s[i]=='p'||s[i]=='P'):(s[i]=='e'||s[i]=='E'));
    if(exponent){++i;if(i<s.size()&&(s[i]=='+'||s[i]=='-'))++i;size_t start=i;while(i<s.size()&&s[i]>='0'&&s[i]<='9')++i;if(i==start)return false;}
    if(hex&&!exponent)return false;
    if(i+1==s.size()&&(s[i]=='f'||s[i]=='F'||s[i]=='d'||s[i]=='D'))s.resize(i);
    return i==s.size();
}
inline std::string store_bits(uint64_t n,unsigned bytes){std::string s(bytes,'\0');for(unsigned i=0;i<bytes;++i)s[i]=char(n>>(8*i));return s;}
inline uint64_t load_bits(const std::string& s,unsigned bytes){if(s.size()!=bytes)return 0;uint64_t n=0;for(unsigned i=0;i<bytes;++i)n|=uint64_t(static_cast<unsigned char>(s[i]))<<(8*i);return n;}
}
