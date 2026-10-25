// Java UTF-16 code units <-> explicitly supported byte encodings.
// kGbk is the project's existing decode table; no external codec dependency.
#pragma once
static int string_encoding(std::string tag) {
    for (char& c : tag) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (tag == "utf-8" || tag == "utf8") return 3;
    if (tag == "ascii" || tag == "us-ascii") return 2;
    if (tag == "iso-8859-1" || tag == "iso8859_1" || tag == "iso8859-1" ||
        tag == "iso_8859_1" || tag == "iso_8859-1" || tag == "latin1") return 1;
    if (tag == "gbk" || tag == "cp936" || tag == "ms936") return 4;
    if (tag == "gb2312" || tag == "gb2312-80" || tag == "euc-cn") return 5;
    return 0;
}
static int gb2312_cp(int a, int b) {
    if (a < 0xa1 || a > 0xf7 || b < 0xa1 || b > 0xfe) return -1;
    if ((a == 0xa2 && b <= 0xaa) || (a == 0xa6 && ((b >= 0xe0 && b <= 0xeb) ||
        (b >= 0xee && b <= 0xf2) || b == 0xf4 || b == 0xf5)) ||
        (a == 0xa8 && (b == 0xbb || b == 0xbd || b == 0xbe || b == 0xc0))) return -1;
    if (a == 0xa1 && b == 0xa4) return 0x30fb;
    if (a == 0xa1 && b == 0xaa) return 0x2015;
    return gbk_cp(a, b);
}
static void append_encoded(std::string& built, const ArrayStorage& elems, int off, int len, int encoding) {
    auto byte = [&](int i) { return elems.get(off + i).i & 255; };
    for (int i = 0; i < len;) {
        int first = byte(i++), cp = first;
        if (encoding == 2 && first > 127) cp = 0xfffd;
        else if (encoding >= 4 && first > 127) {
            cp = -1;
            if (i < len && (encoding != 5 || (first >= 0xa1 && first <= 0xf7 && byte(i) >= 0xa1 && byte(i) <= 0xfe))) {
                cp = encoding == 5 ? gb2312_cp(first, byte(i)) : gbk_cp(first, byte(i));
                if (cp >= 0) ++i;
            }
            if (cp < 0) cp = 0xfffd;
        } else if (encoding == 3 && first >= 128) {
            int remaining = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
            cp = remaining ? first & ((1 << (6 - remaining)) - 1) : 0xfffd;
            for (int j = 0; j < remaining; ++j) {
                if (i >= len) { cp = 0xfffd; break; }
                int b = byte(i);
                if ((b & 0xc0) != 0x80 || (j == 0 && ((first == 0xe0 && b < 0xa0) ||
                    (first == 0xed && b >= 0xa0) || (first == 0xf0 && b < 0x90) || (first == 0xf4 && b >= 0x90)))) {
                    cp = 0xfffd; break;
                }
                ++i; cp = (cp << 6) | (b & 63);
            }
        }
        if (cp > 0xffff) {
            cp -= 0x10000;
            utf8_append(built, 0xd800 | (cp >> 10));
            utf8_append(built, 0xdc00 | (cp & 1023));
        } else utf8_append(built, cp);
    }
}
static std::vector<uint8_t> encode_string(const std::string& text, int encoding) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < text.size();) {
        int cp = utf8_next(text, i);
        if (cp >= 0xd800 && cp <= 0xdfff) {
            size_t next = i;
            int low = utf8_next(text, next);
            if (cp <= 0xdbff && low >= 0xdc00 && low <= 0xdfff) {
                cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00; i = next;
            } else { bytes.push_back('?'); continue; }
        }
        if (encoding == 3) {
            if (cp < 0x80) bytes.push_back(cp);
            else if (cp < 0x800) { bytes.push_back(0xc0 | (cp >> 6)); bytes.push_back(0x80 | (cp & 63)); }
            else {
                if (cp > 0xffff) { bytes.push_back(0xf0 | (cp >> 18)); bytes.push_back(0x80 | ((cp >> 12) & 63)); }
                else bytes.push_back(0xe0 | (cp >> 12));
                bytes.push_back(0x80 | ((cp >> 6) & 63)); bytes.push_back(0x80 | (cp & 63));
            }
        } else if (encoding >= 4 && cp > 127) {
            // Construct once; keep the first mapping within each charset's legal range.
            static const auto reverse = [] {
                std::pair<std::unordered_map<int, uint16_t>, std::unordered_map<int, uint16_t>> maps;
                for (int a = 0x81; a <= 0xfe; ++a) for (int b = 0x40; b <= 0xfe; ++b) {
                    if (b == 0x7f) continue;
                    int c = gbk_cp(a, b);
                    if (c <= 0) continue;
                    maps.first.emplace(c, (a << 8) | b);
                    int c2312 = gb2312_cp(a, b);
                    if (c2312 > 0) maps.second.emplace(c2312, (a << 8) | b);
                }
                return maps;
            }();
            const auto& map = encoding == 5 ? reverse.second : reverse.first;
            auto found = map.find(cp);
            if (found == map.end()) bytes.push_back('?');
            else { bytes.push_back(found->second >> 8); bytes.push_back(found->second & 255); }
        } else bytes.push_back(cp <= (encoding == 1 ? 255 : 127) ? cp : '?');
    }
    return bytes;
}
