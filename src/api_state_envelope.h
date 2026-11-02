#pragma once
#define PIJAVA_API_STATE_ENVELOPE 1
#include "suite_identity.h"
#include <algorithm>
#include <string>
#include <vector>

// An identified wrapper around the unchanged J2S5 VM payload. Never selects
// a classpath or mutates the live VM. The caller must transactionally select
// the accepted API set before validating/restoring any saved instruction PC.
namespace api_state_envelope {
constexpr size_t limit = 256u * 1024u * 1024u;
constexpr size_t prefix_size = 144, header_size = 208;
struct Record { bool legacy=false; std::string api,jar,payload; };
inline bool fingerprint(const std::string& v) {
    return v.size()==64 && std::all_of(v.begin(),v.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
inline std::string digest(const char* prefix,size_t n,const char* payload,size_t bytes) {
    rms_suite_identity_detail::Sha256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(prefix),n);
    sha.update(reinterpret_cast<const uint8_t*>(payload),bytes);
    return sha.finish();
}
inline bool encode(const std::string& api,const std::string& jar,const std::string& payload,
                   std::string& out,std::string& error) {
    error.clear();
    if(!fingerprint(api)||!fingerprint(jar)){error="identity";return false;}
    if(payload.size()<4||payload.compare(0,4,"J2S5")){error="payload_format";return false;}
    if(payload.size()>limit-header_size){error="size";return false;}
    std::string result="J2S6";result.append("\1\0\0\0",4);result+=api;result+=jar;
    const uint64_t size=payload.size();for(unsigned i=0;i<8;++i)result.push_back(char(size>>(8*i)));
    result+=digest(result.data(),result.size(),payload.data(),payload.size());
    result+=payload;out.swap(result);return true;
}
inline bool decode(const std::string& in,const std::vector<std::string>& known,
                   Record& out,std::string& error) {
    out=Record{};error.clear();
    auto reject=[&](const char* reason){error=reason;return false;};
    if(in.size()<4||in.size()>limit)return reject("size");
    if(in.compare(0,4,"J2S5")==0){out.legacy=true;out.payload=in;return true;}
    if(in.compare(0,4,"J2S6"))return reject("format");
    if(in.size()<header_size)return reject("truncated_header");
    if(in.compare(4,4,std::string("\1\0\0\0",4)))return reject("version");
    const std::string api=in.substr(8,64),jar=in.substr(72,64);
    if(!fingerprint(api)||!fingerprint(jar))return reject("identity");
    if(std::find(known.begin(),known.end(),api)==known.end())return reject("unknown_api");
    uint64_t bytes=0;for(unsigned i=0;i<8;++i)bytes|=uint64_t(uint8_t(in[136+i]))<<(8*i);
    if(bytes<4||bytes>limit-header_size||bytes!=in.size()-header_size)return reject("size");
    if(in.compare(header_size,4,"J2S5"))return reject("payload_format");
    // Includes identity/version/length: modifying an API id to another known
    // id must not transplant saved PCs into a different API implementation.
    if(in.substr(prefix_size,64)!=digest(in.data(),prefix_size,in.data()+header_size,size_t(bytes)))
        return reject("payload_hash");
    out.api=api;out.jar=jar;out.payload.assign(in,header_size,size_t(bytes));return true;
}
}
