#include "../src/deterministic_test.h"
#include <cassert>
#include <iostream>
int main(){
    const std::string text=R"({"schema":"J2ME_TEST_RUN_1","mode":"deterministic","step_us":1000,"steps":1000,"epoch_ms":946684800000,"fuel_per_tick":100000,"capture_hz":60,"fixture_seed_hex":"0000000000000000","execution_profile":"reference-v1","rms_policy":"fresh-isolated","payload_policy":"hash-only","snapshot_policy":"reject-before-io","input_tape":"tape.jsonl","output_dir":"output"})";
    r4::Object o;r4::Config c;assert(r4::Parser(text).parse(o)&&r4::config(o,c));
    for(const auto& pair:std::vector<std::pair<const char*,const char*>>{{"step_us","0"},{"steps","-1"},{"epoch_ms","9223372036854775807"},{"epoch_ms","9223372036854775808"},{"capture_hz","1000000"},{"fuel_per_tick","0"}}){auto bad=o;bad[pair.first]={pair.second,false};assert(!r4::config(bad,c));}
    for(auto key:{"schema","mode","rms_policy","snapshot_policy","input_tape","output_dir","execution_profile","fixture_seed_hex"}){auto bad=o;bad[key]={"../invalid",true};assert(!r4::config(bad,c));}
    for(auto s:{"{\"x\":1,\"x\":2}","{\"x\":true}","{\"x\":null}","{\"x\":01}","{\"x\":1.0}","{\"x\":1e3}","{\"x\":\"unterminated}","{\"x\":1} extra"}){r4::Object bad;assert(!r4::Parser(s).parse(bad));}
    r4::Config cfg;std::vector<r4::Event> events;
    assert(r4::events("{\"tick\":0,\"seq\":0,\"action\":\"down\",\"key\":48}\n{\"tick\":1,\"seq\":0,\"action\":\"up\",\"key\":48}\n",cfg,events)&&events.size()==2);
    for(auto s:{"{\"tick\":0,\"seq\":0,\"action\":\"down\",\"key\":48}\n","{\"tick\":0,\"seq\":0,\"action\":\"up\",\"key\":48}\n","{\"tick\":0,\"seq\":0,\"action\":\"down\",\"key\":999}\n"}){events.clear();assert(!r4::events(s,cfg,events));}
    std::cout<<"PASS R4Config: schema/types/duplicates/overflow/path boundaries and balanced input ownership\n";
}
