#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

// Host-only storage experiment. Does not measure game FPS or Switch behavior.
// The legacy loop mirrors the 0.1.67 overlap-copy path for vector<Value>.
int main() {
    constexpr size_t count = 1048576, rounds = 32, shift = 257;
    const char* types[] = {"[B","[Z","[C","[S","[I","[F","[J","[D","[Ljava/lang/Object;"};
    std::cout << "{\n  \"version\": \"" << kVer << "\",\n  \"scope\": \"host array payload and synthetic overlap copy; not Switch FPS\",\n";
    std::cout << "  \"elements\": " << count << ",\n  \"value_bytes\": " << sizeof(Value)
              << ",\n  \"legacy_array_container_bytes\": " << sizeof(std::vector<Value>)
              << ",\n  \"compact_array_container_bytes\": " << sizeof(ArrayStorage) << ",\n  \"arrays\": [\n";
    for (size_t i=0;i<9;++i) {
        ArrayStorage a;
        if (!a.reset(types[i],count)) return 1;
        std::cout << "    {\"descriptor\": \"" << types[i] << "\", \"legacy_payload_bytes\": " << count*sizeof(Value)
                  << ", \"compact_payload_bytes\": " << a.payload_bytes() << ", \"compact_capacity_bytes\": " << a.capacity_bytes() << "}" << (i==8?"\n":",\n");
    }
    std::cout << "  ],\n  \"byte_overlap_copy\": {\"rounds\": " << rounds << ", \"samples\": [\n";
    for (int trial=0;trial<3;++trial) {
        std::vector<Value> old(count);
        ArrayStorage packed; if (!packed.reset("[B",count)) return 1;
        for (size_t i=0;i<count;++i) { int v=int(i&255); if (v>=128)v-=256;old[i]=Value::of_i(v); packed.set(i,Value::of_i(v)); }
        auto legacy = [&]() {
            auto start=std::chrono::steady_clock::now();
            for (size_t r=0;r<rounds;++r)
                for (size_t i=count-shift;i-- >0;) old[shift+i]=old[i];
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        };
        auto compact = [&]() {
            auto start=std::chrono::steady_clock::now();
            for(size_t r=0;r<rounds;++r) if(!packed.copy_from(packed,0,shift,count-shift)) std::exit(2);
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        };
        int64_t a,b;
        if(trial&1){b=compact();a=legacy();}else{a=legacy();b=compact();}
        uint64_t sum=0;
        for(size_t i=0;i<count;++i) { if(old[i].i != packed.get(i).i) return 3; sum += (uint32_t)packed.get(i).i; }
        std::cout << "    {\"legacy_us\": "<<a<<", \"compact_us\": "<<b<<", \"checksum\": "<<sum<<", \"contents_equal\": true}"<<(trial==2?"\n":",\n");
    }
    std::cout<<"  ]}\n}\n";
}
