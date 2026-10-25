#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>

int main() {
    const std::string directory = "tests/fixtures/jpeg/";
    struct Case { const char* name; int width, height; };
    for (const Case& item : {Case{"baseline",17,11}, {"progressive",24,19}, {"grayscale",9,7}, {"cmyk",7,5}}) {
        std::vector<uint8_t> bytes, reference, rgb, alpha;
        assert(read_whole(directory + item.name + ".jpg", bytes));
        assert(read_whole(directory + item.name + ".rgb", reference));
        int w = 0, h = 0;std::string error;
        assert(decode_image(bytes, rgb, alpha, w, h, error));
        assert(w == item.width && h == item.height && rgb.size() == reference.size());
        assert(alpha == std::vector<uint8_t>(size_t(w) * h, 255));
        int maximum = 0;size_t sum = 0;
        for (size_t i = 0; i < rgb.size(); ++i) {
            const int delta = std::abs(int(rgb[i]) - int(reference[i]));
            maximum = std::max(maximum, delta);sum += size_t(delta);
        }
        assert(maximum <= 5 && double(sum) / rgb.size() < 1.0);
        // JPEG EOI may legally be followed by application bytes.
        bytes.insert(bytes.end(), {1,2,3});
        assert(decode_image(bytes, rgb, alpha, w, h, error));
        std::cout << "JPEG " << item.name << " max_delta=" << maximum << " mean_delta=" << double(sum)/rgb.size() << "\n";
    }
    for (const char* name : {"truncated", "oversized"}) {
        std::vector<uint8_t> bytes, rgb={7}, alpha={9};int w=11,h=12;std::string error;
        assert(read_whole(directory + name + ".jpg", bytes));
        assert(!decode_image(bytes, rgb, alpha, w, h, error));
        assert(!error.empty() && rgb == std::vector<uint8_t>{7} && alpha == std::vector<uint8_t>{9} && w==11 && h==12);
    }
    // Exercise small malformed marker/header inputs with output sentinels.
    for (size_t size=0;size<128;++size) {
        std::vector<uint8_t> bytes(size,0xff),rgb,alpha;int w=0,h=0;std::string error;
        if(size>1)bytes[1]=0xd8;if(size>3)bytes[size-1]=0xd9;
        assert(!decode_image(bytes,rgb,alpha,w,h,error));
    }
    for (const char* name : {"baseline", "progressive", "grayscale", "truncated", "oversized"}) {
        std::vector<uint8_t> bytes;assert(read_whole(directory + name + ".jpg",bytes));
        VMS_CL(g_jar_files)[std::string("jpeg-") + name + ".jpg"] = std::move(bytes);
    }
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    int cid=ensure_class("JpegImageChecks");
    int count=run(find_method(cid,"run","()I",false),{}).i;
    assert(!VMS_EXEC(g_thrown) && count==383);
    std::cout << "PASS JpegImageChecks " << count << " Java resource, stream, slice, opaque pixels, immutability and exception checks\n";
}
