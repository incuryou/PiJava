#pragma once
#include "optimization_config.h"
#include "render3d/jobs.h"
#include "puff_init.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "jpeg_decoder.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <climits>
#include <cstring>

// Pure owned decoding. This module has no VM/Java/cache identity or GPU access.
namespace native_image {
struct Diagnostics { uint64_t png_fast=0,png_fallback=0; };
struct Result : render3d::PrepOutput {
    std::vector<uint8_t> rgb,alpha;
    int width=0,height=0,worker_slot=-1;
    std::string error;
    bool ok=false,cpu_clock_available=false;
    Diagnostics diagnostics;
    uint64_t worker_cpu_us=0,worker_wall_us=0;
};
struct RunMetrics { bool used_worker=false;size_t input_bytes=0,reserved_bytes=0;uint64_t wait_us=0; };
namespace totals {
inline std::atomic<uint64_t> worker_jobs{0},local_jobs{0},input_bytes{0},wait_us{0},worker_cpu_us{0},worker_wall_us{0};
inline void reset(){worker_jobs=local_jobs=input_bytes=wait_us=worker_cpu_us=worker_wall_us=0;}
}
inline void record_run(const Result& result,const RunMetrics& metrics){
    ++(metrics.used_worker?totals::worker_jobs:totals::local_jobs);
    totals::input_bytes+=metrics.input_bytes;totals::wait_us+=metrics.wait_us;
    totals::worker_cpu_us+=result.worker_cpu_us;totals::worker_wall_us+=result.worker_wall_us;
}
inline std::string summary(){
    return "NATIVEIMG schema=1 worker_jobs="+std::to_string(totals::worker_jobs.load())+" local_jobs="+std::to_string(totals::local_jobs.load())+
        " input_bytes="+std::to_string(totals::input_bytes.load())+" wait_us="+std::to_string(totals::wait_us.load())+
        " worker_cpu_us="+std::to_string(totals::worker_cpu_us.load())+" worker_wall_us="+std::to_string(totals::worker_wall_us.load());
}
inline uint32_t read_be32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
inline bool decode_png(const std::vector<uint8_t>& png, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, int& w, int& h, std::string& err, Diagnostics& diagnostics) {
    static const uint8_t signature[8]={137,80,78,71,13,10,26,10};
    if(png.size()<8||std::memcmp(png.data(),signature,8)!=0){err="not png";return false;}
    int bit_depth = 0, color = 0, interlace = 0;
    std::vector<uint8_t> idat, plte, trns;
    size_t i = 8;
    while (i + 12 <= png.size()) {
        uint32_t n = read_be32(&png[i]);
        if (n > png.size() - i - 12) { err = "bad png chunk"; return false; }
        std::string type((const char*)&png[i + 4], 4);
        const uint8_t* body = &png[i + 8];
        if(type=="IHDR") {
            if(n!=13||body[10]!=0||body[11]!=0||body[12]>1){err="bad png header";return false;}
            w=(int)read_be32(body);h=(int)read_be32(body+4);bit_depth=body[8];color=body[9];interlace=body[12];
        }
        else if (type == "PLTE") plte.assign(body, body + n);
        else if (type == "tRNS") trns.assign(body, body + n);
        else if (type == "IDAT") idat.insert(idat.end(), body, body + n);
        else if (type == "IEND") break;
        i += 12 + n;
    }
    int channels = color == 2 ? 3 : color == 4 ? 2 : color == 6 ? 4 : 1;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || (color != 0 && color != 2 && color != 3 && color != 4 && color != 6)) {
        err = "png format c" + std::to_string(color) + " d" + std::to_string(bit_depth) + " i" + std::to_string(interlace);
        return false;
    }
    if ((color == 2 || color == 4 || color == 6) && bit_depth != 8) { err = "png depth"; return false; }
    if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8) { err = "png depth"; return false; }
    // PNG permits all zlib window sizes up to 32 KiB, not only CMF 0x78.
    if (idat.size() < 8 || (idat[0] & 15) != 8 || (idat[0] >> 4) > 7 ||
        (((unsigned)idat[0] << 8) | idat[1]) % 31 != 0 || (idat[1] & 32)) {
        err = "png zlib"; return false;
    }
    static const int a7r[7] = {0, 0, 4, 0, 2, 0, 1};
    static const int a7c[7] = {0, 4, 0, 2, 0, 1, 0};
    static const int a7ri[7] = {8, 8, 8, 4, 4, 2, 2};
    static const int a7ci[7] = {8, 8, 4, 4, 2, 2, 1};
    int passes = interlace ? 7 : 1;
    auto pass_dim = [&](int pass, int& rows, int& cols) {
        if (!interlace) { rows = h; cols = w; return; }
        rows = h > a7r[pass] ? (h - a7r[pass] + a7ri[pass] - 1) / a7ri[pass] : 0;
        cols = w > a7c[pass] ? (w - a7c[pass] + a7ci[pass] - 1) / a7ci[pass] : 0;
    };
    size_t raw_size = 0;
    for (int pass = 0; pass < passes; ++pass) {
        int rows = 0, cols = 0; pass_dim(pass, rows, cols);
        if (!rows || !cols) continue;
        raw_size += (size_t)rows * (1 + (cols * channels * bit_depth + 7) / 8);
    }
    std::vector<uint8_t> raw(raw_size ? raw_size : 1);
    unsigned long dlen = raw.size(), slen = idat.size() - 6;
    bool inflated=false;
#if J2ME_OPT_FAST_PNG
    // Reuse the already bundled stb decoder's bounded Huffman fast table.
    // Keep the existing PNG parser (palettes, Adam7, filters and tRNS), and
    // the strict ZIP decompressor unchanged. Adler mismatch/unsupported input
    // falls back to puff rather than silently changing legacy compatibility.
    if(raw_size<=size_t(INT_MAX) && slen<=unsigned(INT_MAX)) {
        std::lock_guard<std::mutex> decoder_lock(j2me_image::decoder_mutex());
        const int got=stbi_zlib_decode_noheader_buffer(reinterpret_cast<char*>(raw.data()),int(raw_size),
            reinterpret_cast<const char*>(idat.data()+2),int(slen));
        if(got>=0 && size_t(got)==raw_size) {
            uint32_t a=1,b=0;
            for(size_t off=0;off<raw_size;) {
                const size_t end=std::min(raw_size,off+5552);
                while(off<end){a+=raw[off++];b+=a;}
                a%=65521u;b%=65521u;
            }
            inflated=((b<<16)|a)==read_be32(idat.data()+idat.size()-4);
            if(inflated)++diagnostics.png_fast;
        }
    }
#endif
    if(!inflated) {
        ++diagnostics.png_fallback;
        if (j2me_image::inflate_raw(raw.data(), &dlen, idat.data() + 2, &slen) != 0 || dlen < raw_size) { err = "png inflate"; return false; }
    }
    rgb.assign((size_t)w * h * 3, 0);
    alpha.assign((size_t)w * h, 255);
    auto paeth = [](int a, int b, int c) {
        int pp = a + b - c, pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
        if (pa <= pb && pa <= pc) return a;
        return pb <= pc ? b : c;
    };
    size_t p = 0;
    for (int pass = 0; pass < passes; ++pass) {
        int rows = 0, cols = 0; pass_dim(pass, rows, cols);
        if (!rows || !cols) continue;
        int row_bytes = (cols * channels * bit_depth + 7) / 8;
        int filter_bpp = std::max(1, (channels * bit_depth) / 8);
        std::vector<uint8_t> prev(row_bytes, 0), cur(row_bytes, 0);
        for (int ry = 0; ry < rows; ++ry) {
            if (p >= raw.size()) { err = "png short"; return false; }
            int filter = raw[p++];
            for (int x = 0; x < row_bytes; ++x) {
                int v = raw[p++], a = x >= filter_bpp ? cur[x - filter_bpp] : 0, b = prev[x], c = x >= filter_bpp ? prev[x - filter_bpp] : 0;
                if (filter == 1) v += a; else if (filter == 2) v += b; else if (filter == 3) v += (a + b) / 2; else if (filter == 4) v += paeth(a, b, c);
                else if (filter) { err = "png filter"; return false; }
                cur[x] = (uint8_t)v;
            }
            auto sample = [&](int x) {
                int bit = x * bit_depth;
                int byte = cur[bit >> 3];
                int shift = 8 - bit_depth - (bit & 7);
                return (byte >> shift) & ((1 << bit_depth) - 1);
            };
            for (int rx = 0; rx < cols; ++rx) {
                int r = 0, g = 0, b = 0, a = 255, s = sample(rx);
                if (color == 0) {
                    r = g = b = bit_depth == 8 ? s : s * 255 / ((1 << bit_depth) - 1);
                    if (trns.size() >= 2 && s == (int)((trns[0] << 8) | trns[1])) a = 0;
                } else if (color == 2) {
                    r = cur[rx * 3]; g = cur[rx * 3 + 1]; b = cur[rx * 3 + 2];
                    if (trns.size() >= 6 && r == trns[1] && g == trns[3] && b == trns[5]) a = 0;
                } else if (color == 3) {
                    if (s * 3 + 2 < (int)plte.size()) { r = plte[s * 3]; g = plte[s * 3 + 1]; b = plte[s * 3 + 2]; }
                    if (s < (int)trns.size()) a = trns[s];
                } else if (color == 4) {
                    r = g = b = cur[rx * 2]; a = cur[rx * 2 + 1];
                } else {
                    r = cur[rx * 4]; g = cur[rx * 4 + 1]; b = cur[rx * 4 + 2]; a = cur[rx * 4 + 3];
                }
                int x = interlace ? a7c[pass] + rx * a7ci[pass] : rx;
                int y = interlace ? a7r[pass] + ry * a7ri[pass] : ry;
                size_t di = ((size_t)y * w + x);
                rgb[di * 3] = (uint8_t)r; rgb[di * 3 + 1] = (uint8_t)g; rgb[di * 3 + 2] = (uint8_t)b;
                alpha[di] = (uint8_t)a;
            }
            prev.swap(cur);
        }
    }
    return true;
}
inline bool gif_subblocks(const std::vector<uint8_t>& g, size_t& p, std::vector<uint8_t>& out, std::string& err) {
    out.clear();
    while (p < g.size()) {
        int n = g[p++];
        if (n == 0) return true;
        if (p + (size_t)n > g.size()) { err = "gif sub"; return false; }
        out.insert(out.end(), g.begin() + p, g.begin() + p + n);
        p += n;
    }
    err = "gif trunc";
    return false;
}
inline bool gif_error(std::string* error,const char* text){if(error)*error=text;return false;}
// Admission and decoding share the same extension boundaries, including the old
// finite GCE tolerance: non-four-byte bodies and a missing zero terminator are
// accepted when the declared body is present. Other extensions require subblocks.
inline bool gif_skip_extension(const std::vector<uint8_t>& gif,size_t& p,int& transparent,std::string* error=nullptr){
    if(p>=gif.size())return gif_error(error,"gif ext");
    const uint8_t label=gif[p++];
    if(label==0xf9){
        if(p>=gif.size())return gif_error(error,"gif gce");
        const size_t count=gif[p++];
        if(count>gif.size()-p)return gif_error(error,"gif gce");
        if(count>=4&&(gif[p]&1))transparent=gif[p+3];
        p+=count;if(p<gif.size()&&gif[p]==0)++p;
        return true;
    }
    while(p<gif.size()){
        const size_t count=gif[p++];if(!count)return true;
        if(count>gif.size()-p)return gif_error(error,"gif sub");
        p+=count;
    }
    return gif_error(error,"gif trunc");
}
struct GifFrame {int left=0,top=0,width=0,height=0,packed=0,canvas_width=0,canvas_height=0;};
inline bool gif_read_frame(const std::vector<uint8_t>& gif,size_t& p,int screen_width,int screen_height,GifFrame& frame,std::string* error=nullptr){
    if(p>gif.size()||gif.size()-p<9)return gif_error(error,"gif img");
    auto word=[&](size_t at){return int(gif[at])|(int(gif[at+1])<<8);};
    frame.left=word(p);frame.top=word(p+2);frame.width=word(p+4);frame.height=word(p+6);frame.packed=gif[p+8];
    if(!frame.width||!frame.height)return gif_error(error,"gif img");
    if(screen_width>1024||screen_height>1024||frame.width>1024||frame.height>1024||
       frame.left>1024-frame.width||frame.top>1024-frame.height)return gif_error(error,"gif size");
    frame.canvas_width=std::max(screen_width,frame.left+frame.width);
    frame.canvas_height=std::max(screen_height,frame.top+frame.height);
    p+=9;return true;
}
inline bool decode_gif(const std::vector<uint8_t>& gif, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, int& w, int& h, std::string& err) {
    if (gif.size() < 13 || gif[0] != 'G' || gif[1] != 'I' || gif[2] != 'F') { err = "not gif"; return false; }
    int sw = gif[6] | (gif[7] << 8), sh = gif[8] | (gif[9] << 8);
    if(sw>1024||sh>1024){err="gif size";return false;}
    int packed = gif[10];
    size_t p = 13;
    std::vector<uint8_t> gct;
    if (packed & 0x80) {
        int n = 1 << ((packed & 7) + 1);
        if (p + (size_t)n * 3 > gif.size()) { err = "gif gct"; return false; }
        gct.assign(gif.begin() + p, gif.begin() + p + n * 3);
        p += (size_t)n * 3;
    }
    int trans = -1, iw = 0, ih = 0, left = 0, top = 0, interlace = 0;
    std::vector<uint8_t> indices, palette = gct;
    bool got = false;
    while (p < gif.size() && !got) {
        uint8_t tag = gif[p++];
        if (tag == 0x3B) break;
        if (tag == 0x21) {
            if(!gif_skip_extension(gif,p,trans,&err))return false;
        } else if (tag == 0x2C) {
            // Validate the REAL descriptor before LCT/data/dictionary/indices allocation.
            GifFrame frame;if(!gif_read_frame(gif,p,sw,sh,frame,&err))return false;
            left=frame.left;top=frame.top;iw=frame.width;ih=frame.height;const int pk=frame.packed;
            interlace = (pk & 0x40) ? 1 : 0;
            palette = gct;
            if (pk & 0x80) {
                int n = 1 << ((pk & 7) + 1);
                if (p + (size_t)n * 3 > gif.size()) { err = "gif lct"; return false; }
                palette.assign(gif.begin() + p, gif.begin() + p + n * 3);
                p += (size_t)n * 3;
            }
            if (p >= gif.size() || iw <= 0 || ih <= 0 || palette.size() < 3) { err = "gif img"; return false; }
            int min_code = gif[p++];
            if (min_code < 2 || min_code > 8) { err = "gif lzw"; return false; }
            std::vector<uint8_t> data;
            if (!gif_subblocks(gif, p, data, err)) return false;
            int clear = 1 << min_code, eoi = clear + 1, code_size = min_code + 1, next = eoi + 1;
            std::vector<std::vector<uint8_t>> dict(4096);
            for (int i = 0; i < clear; ++i) dict[i] = {(uint8_t)i};
            uint32_t acc = 0; int bits = 0, di = 0, prev = -1;
            auto read_code = [&]() -> int {
                while (bits < code_size) {
                    if (di >= (int)data.size()) return eoi;
                    acc |= (uint32_t)data[di++] << bits;
                    bits += 8;
                }
                int code = (int)(acc & ((1u << code_size) - 1));
                acc >>= code_size; bits -= code_size;
                return code;
            };
            indices.clear();
            indices.reserve((size_t)iw * ih);
            while ((int)indices.size() < iw * ih) {
                int code = read_code();
                if (code == eoi) break;
                if (code == clear) {
                    code_size = min_code + 1; next = eoi + 1; prev = -1;
                    for (int i = eoi + 1; i < 4096; ++i) dict[i].clear();
                    continue;
                }
                std::vector<uint8_t> entry;
                if (code < next && !dict[code].empty()) entry = dict[code];
                else if (code == next && prev >= 0) { entry = dict[prev]; entry.push_back(dict[prev][0]); }
                else { err = "gif code"; return false; }
                indices.insert(indices.end(), entry.begin(), entry.end());
                if (prev >= 0 && next < 4096) {
                    dict[next] = dict[prev];
                    dict[next].push_back(entry[0]);
                    next++;
                    if (next == (1 << code_size) && code_size < 12) code_size++;
                }
                prev = code;
            }
            got = true;
        } else { err = "gif block"; return false; }
    }
    if (!got) { err = "gif empty"; return false; }
    w = std::max(sw, left + iw); h = std::max(sh, top + ih);
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) { err = "gif size"; return false; }
    rgb.assign((size_t)w * h * 3, 0);
    alpha.assign((size_t)w * h, 0);
    auto put = [&](int x, int y, int index) {
        if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
        size_t di = (size_t)y * w + x;
        if (index == trans) { alpha[di] = 0; return; }
        int pi = index * 3;
        if (pi + 2 < (int)palette.size()) { rgb[di * 3] = palette[pi]; rgb[di * 3 + 1] = palette[pi + 1]; rgb[di * 3 + 2] = palette[pi + 2]; }
        alpha[di] = 255;
    };
    if (!interlace) {
        for (int y = 0; y < ih; ++y)
            for (int x = 0; x < iw; ++x) {
                size_t k = (size_t)y * iw + x;
                put(left + x, top + y, k < indices.size() ? indices[k] : 0);
            }
    } else {
        int starts[4] = {0, 4, 2, 1}, steps[4] = {8, 8, 4, 2}, row = 0;
        for (int pass = 0; pass < 4; ++pass)
            for (int y = starts[pass]; y < ih; y += steps[pass], ++row)
                for (int x = 0; x < iw; ++x) {
                    size_t k = (size_t)row * iw + x;
                    put(left + x, top + y, k < indices.size() ? indices[k] : 0);
                }
    }
    return true;
}
inline bool decode_raw(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, int& w, int& h, std::string& err, Diagnostics& diagnostics) {
    if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8)
        return j2me_image::decode_jpeg(bytes, rgb, alpha, w, h, err);
    std::string png_err, gif_err;
    if (decode_png(bytes, rgb, alpha, w, h, png_err, diagnostics)) return true;
    if (decode_gif(bytes, rgb, alpha, w, h, gif_err)) return true;
    err = png_err + "; " + gif_err;
    return false;
}
inline void decode_sync(const std::vector<uint8_t>& bytes,Result& result){
    result.ok=decode_raw(bytes,result.rgb,result.alpha,result.width,result.height,result.error,result.diagnostics);
}
// Admission scans only container headers, never entropy or Java objects. Unknown, excessive,
// ambiguous and tiny inputs stay on the original synchronous path, with its original limits.
inline size_t reservation_bytes(const std::vector<uint8_t>& b,size_t minimum_pixels=4096){
    uint64_t width=0,height=0;
    if(b.size()>=24&&b[0]==137&&std::memcmp(b.data()+1,"PNG\r\n\x1a\n",7)==0){
        bool header=false;size_t p=8;
        while(p+12<=b.size()){
            const size_t n=read_be32(b.data()+p);if(n>b.size()-p-12)return 0;
            if(std::memcmp(b.data()+p+4,"IHDR",4)==0){if(header||n!=13)return 0;header=true;width=read_be32(b.data()+p+8);height=read_be32(b.data()+p+12);}
            if(std::memcmp(b.data()+p+4,"IEND",4)==0)break;
            p+=n+12;
        }
        if(!header||width>4096||height>4096)return 0;
    }else if(b.size()>=13&&std::memcmp(b.data(),"GIF",3)==0){
        auto le16=[&](size_t p){return unsigned(b[p])|(unsigned(b[p+1])<<8);};
        const int screen_width=int(le16(6)),screen_height=int(le16(8));
        if(screen_width>1024||screen_height>1024)return 0;
        size_t p=13;
        if(b[10]&128){const size_t palette=size_t(3)<<(1+(b[10]&7));if(palette>b.size()-p)return 0;p+=palette;}
        bool found=false;
        while(p<b.size()){
            const auto tag=b[p++];
            if(tag==0x2c){
                GifFrame frame;if(!gif_read_frame(b,p,screen_width,screen_height,frame))return 0;
                width=frame.canvas_width;height=frame.canvas_height;
                found=true;break;
            }
            if(tag!=0x21)return 0;
            int transparent=-1;if(!gif_skip_extension(b,p,transparent))return 0;
        }
        if(!found||width>1024||height>1024)return 0;
    }else if(b.size()>=4&&b[0]==0xff&&b[1]==0xd8){
        size_t p=2;
        while(p<b.size()){
            if(b[p++]!=0xff)return 0;
            while(p<b.size()&&b[p]==0xff)++p;
            if(p==b.size())return 0;
            const unsigned marker=b[p++];if(marker==0xd9||marker==0xda)break;
            if(marker==0x01||(marker>=0xd0&&marker<=0xd8))continue;
            if(b.size()-p<2)return 0;
            const size_t n=(size_t(b[p])<<8)|b[p+1];if(n<2||n>b.size()-p)return 0;
            if(marker>=0xc0&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc){
                if(n<8)return 0;
                height=(unsigned(b[p+3])<<8)|b[p+4];width=(unsigned(b[p+5])<<8)|b[p+6];break;
            }
            p+=n;
        }
        if(width>1024||height>1024)return 0;
    }else return 0;
    const uint64_t pixels=width*height;if(!width||!height||pixels<minimum_pixels)return 0;
    // Conservative peak covers the input copy/capacity, parsed chunks, RGB/alpha, PNG rows/raw,
    // JPEG progressive coefficient planes, GIF dictionary/indices, allocation and stack metadata.
    const uint64_t bytes=uint64_t(b.size())*8+pixels*32+1024*1024;
    return bytes<=16*1024*1024?size_t(bytes):0;
}
inline bool current_cpu_us(uint64_t& out){
#ifdef __SWITCH__
    u64 ticks=0;if(R_FAILED(svcGetInfo(&ticks,InfoType_ThreadTickCount,CUR_THREAD_HANDLE,-1)))return false;out=armTicksToNs(ticks)/1000;return true;
#elif defined(_WIN32)
    FILETIME c{},e{},k{},u{};if(!GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u))return false;out=(((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime))/10;return true;
#else
    timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))return false;out=uint64_t(t.tv_sec)*1000000+uint64_t(t.tv_nsec)/1000;return true;
#endif
}
struct DecodeWork : render3d::PrepWork {
    const std::vector<uint8_t> bytes;
    explicit DecodeWork(const std::vector<uint8_t>& input):bytes(input){}
    bool run(const render3d::PrepCancel& cancel,std::unique_ptr<render3d::PrepOutput>& output)override{
        if(cancel.requested())return false;
        std::unique_ptr<Result> result(new Result);result->worker_slot=p7::t_slot;
        const auto started=p7::ticks();uint64_t cpu_start=0,cpu_end=0;const bool cpu_start_ok=current_cpu_us(cpu_start);
        {P7_SCOPE(Decode);decode_sync(bytes,*result);}
        const bool cpu_end_ok=current_cpu_us(cpu_end);result->worker_wall_us=p7::ticks_to_us(p7::ticks()-started);
        result->cpu_clock_available=cpu_start_ok&&cpu_end_ok;result->worker_cpu_us=result->cpu_clock_available&&cpu_end>=cpu_start?cpu_end-cpu_start:0;
        output=std::move(result);return true; // decode errors are owned RESULTS, not scheduler failure
    }
};
inline render3d::PrepSubmitResult try_submit(render3d::PrepQueue& queue,render3d::Epoch epoch,const std::vector<uint8_t>& bytes,size_t minimum_pixels=4096){
    const size_t reservation=reservation_bytes(bytes,minimum_pixels);
    if(!reservation)return {render3d::PrepSubmitCode::Invalid,0};
    if(!j2me_image::initialize_puff())return {render3d::PrepSubmitCode::Invalid,0};
    render3d::PrepJob job;job.epoch=epoch;job.priority=render3d::PrepPriority::CurrentFrame;job.reserved_bytes=reservation;
#if defined(__cpp_exceptions)
    try{
#endif
        job.work.reset(new DecodeWork(bytes));return queue.try_submit(std::move(job));
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){return {render3d::PrepSubmitCode::OutOfMemory,0};}
#endif
}
inline void decode_wait(render3d::PrepQueue* queue,render3d::Epoch epoch,const std::vector<uint8_t>& bytes,Result& result,RunMetrics& metrics){
    result=Result{};metrics=RunMetrics{};
    metrics.input_bytes=bytes.size();
    if(queue){
        auto submitted=try_submit(*queue,epoch,bytes);
        if(submitted.code==render3d::PrepSubmitCode::Accepted){
            const auto started=p7::ticks();render3d::PrepResult ready;
            const auto waited=queue->wait_take(submitted.sequence,epoch,ready);metrics.wait_us=p7::ticks_to_us(p7::ticks()-started);
            if(waited==render3d::PrepWaitCode::Taken&&ready.code==render3d::PrepResultCode::Succeeded&&ready.output){
                metrics.used_worker=true;metrics.reserved_bytes=reservation_bytes(bytes);result=std::move(*static_cast<Result*>(ready.output.get()));record_run(result,metrics);return;
            }
        }
    }
    // Cancellation, disabled domain, small work and budget pressure retain the original native
    // synchronous escape path. No job is stolen, no Java runs and no new Java exception is invented.
    P7_SCOPE(Decode);decode_sync(bytes,result);record_run(result,metrics);
}
} // namespace native_image
