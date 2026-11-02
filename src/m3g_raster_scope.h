#pragma once
#include <cstdint>
// Main-only scope control. No Java references, worker pointers or serialized PCs.
namespace m3g_raster_scope {
enum class Boundary:uint8_t {Fallback,Return,Exception,Yield,Reentry,Lifecycle,Save,Shutdown};
inline bool enabled=false,has_pending=false;
inline bool (*flush_callback)(uint64_t,Boundary)=nullptr;
inline bool (*flush_all_callback)(Boundary)=nullptr;
inline uint64_t (*query_callback)()=nullptr;
inline uint64_t sequence=0;
inline uint64_t next_token(){if(!++sequence)++sequence;return sequence;}
inline uint64_t current(){return enabled&&query_callback?query_callback():0;}
inline bool flush_all(Boundary reason);
inline bool flush(uint64_t token,Boundary reason){return !has_pending||(!token?flush_all(reason):flush_callback&&flush_callback(token,reason));}
inline bool flush_all(Boundary reason){return !has_pending||(flush_all_callback&&flush_all_callback(reason));}
}
