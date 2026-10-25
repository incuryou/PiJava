// Included after j2mevm.cpp and check(); all fixtures are built without filesystem I/O.
struct ZipCheckEntry {
    std::string name;
    std::vector<uint8_t> raw;
    uint16_t method = 0;
    int descriptor = 0; // 0, unsigned 12-byte, or signed 16-byte descriptor.
    bool local_sizes = false;
};
struct ZipCheckFixture {
    std::vector<uint8_t> bytes;
    std::vector<size_t> locals, data, centrals;
    size_t central = 0, eocd = 0;
};
static void zip_check_u16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(value & 255); bytes.push_back(value >> 8);
}
static void zip_check_u32(std::vector<uint8_t>& bytes, uint32_t value) {
    zip_check_u16(bytes, value & 65535); zip_check_u16(bytes, value >> 16);
}
static void zip_check_set32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (int i = 0; i < 4; i++) bytes[offset + i] = (value >> (i * 8)) & 255;
}
static ZipCheckFixture zip_check_build(const std::vector<ZipCheckEntry>& entries,
        bool reverse = false, const std::vector<uint8_t>& comment = {}) {
    ZipCheckFixture fixture;
    auto& bytes = fixture.bytes;
    std::vector<std::vector<uint8_t>> compressed;
    std::vector<uint32_t> crcs;
    for (const auto& entry : entries) {
        std::vector<uint8_t> packed = entry.raw;
        if (entry.method == 8) {
            // A literal fixed-Huffman block with a back-reference for abc repeated six times.
            check(std::string(entry.raw.begin(), entry.raw.end()) == "abcabcabcabcabcabc", "deflate fixture payload");
            packed = {0x4b,0x4c,0x4a,0x4e,0x44,0x45,0x00};
        }
        compressed.push_back(packed);
        uint32_t crc = crc32(entry.raw.data(), entry.raw.size());
        crcs.push_back(crc);
        fixture.locals.push_back(bytes.size());
        zip_check_u32(bytes, 0x04034b50);
        zip_check_u16(bytes, 20);
        zip_check_u16(bytes, entry.descriptor ? 8 : 0);
        zip_check_u16(bytes, entry.method);
        zip_check_u32(bytes, 0); // DOS time/date.
        bool known = !entry.descriptor || entry.local_sizes;
        zip_check_u32(bytes, known ? crc : 0);
        zip_check_u32(bytes, known ? packed.size() : 0);
        zip_check_u32(bytes, known ? entry.raw.size() : 0);
        zip_check_u16(bytes, entry.name.size()); zip_check_u16(bytes, 0);
        bytes.insert(bytes.end(), entry.name.begin(), entry.name.end());
        fixture.data.push_back(bytes.size());
        bytes.insert(bytes.end(), packed.begin(), packed.end());
        if (entry.descriptor) {
            if (entry.descriptor == 16) zip_check_u32(bytes, 0x08074b50);
            zip_check_u32(bytes, crc);
            zip_check_u32(bytes, packed.size());
            zip_check_u32(bytes, entry.raw.size());
        }
    }
    fixture.central = bytes.size();
    fixture.centrals.resize(entries.size());
    for (size_t sequence = 0; sequence < entries.size(); sequence++) {
        size_t index = reverse ? entries.size() - sequence - 1 : sequence;
        const auto& entry = entries[index];
        fixture.centrals[index] = bytes.size();
        zip_check_u32(bytes, 0x02014b50);
        zip_check_u16(bytes, 20); zip_check_u16(bytes, 20);
        zip_check_u16(bytes, entry.descriptor ? 8 : 0);
        zip_check_u16(bytes, entry.method);
        zip_check_u32(bytes, 0);
        zip_check_u32(bytes, crcs[index]);
        zip_check_u32(bytes, compressed[index].size());
        zip_check_u32(bytes, entry.raw.size());
        zip_check_u16(bytes, entry.name.size());
        zip_check_u16(bytes, 0); zip_check_u16(bytes, 0); zip_check_u16(bytes, 0);
        zip_check_u16(bytes, 0); zip_check_u32(bytes, 0);
        zip_check_u32(bytes, fixture.locals[index]);
        bytes.insert(bytes.end(), entry.name.begin(), entry.name.end());
    }
    fixture.eocd = bytes.size();
    zip_check_u32(bytes, 0x06054b50);
    zip_check_u16(bytes, 0); zip_check_u16(bytes, 0);
    zip_check_u16(bytes, entries.size()); zip_check_u16(bytes, entries.size());
    zip_check_u32(bytes, fixture.eocd - fixture.central);
    zip_check_u32(bytes, fixture.central);
    zip_check_u16(bytes, comment.size());
    bytes.insert(bytes.end(), comment.begin(), comment.end());
    return fixture;
}
static void run_zip_native_checks() {
    int assertions = 0;
    auto good = [&](const ZipCheckFixture& fixture, const std::vector<ZipCheckEntry>& entries, const char* reason) {
        std::unordered_map<std::string, std::vector<uint8_t>> output;
        std::string error;
        check(unzip(fixture.bytes, output, error), reason); assertions++;
        check(error.empty(), "successful ZIP has no error"); assertions++;
        check(output.size() == entries.size(), "every ZIP entry extracted"); assertions++;
        for (const auto& entry : entries) {
            check(output.count(entry.name) == 1 && output[entry.name] == entry.raw, "ZIP filename and payload preserved"); assertions++;
        }
    };
    auto bad = [&](const ZipCheckFixture& fixture, const char* reason) {
        std::unordered_map<std::string, std::vector<uint8_t>> output;
        std::string error;
        check(!unzip(fixture.bytes, output, error), reason); assertions++;
        check(!error.empty(), "rejected ZIP has a diagnostic"); assertions++;
    };
    ZipCheckEntry stored{"META-INF/MANIFEST.MF", {'M','I','D','l','e','t','-','1',':',' ','X',',',',','M','\n'}};
    ZipCheckEntry deflated{"Main.class", {'a','b','c','a','b','c','a','b','c','a','b','c','a','b','c','a','b','c'}, 8};
    good(zip_check_build({stored, deflated}), {stored, deflated}, "stored and deflated entries");
    stored.descriptor = 16; deflated.descriptor = 16;
    good(zip_check_build({stored, deflated}), {stored, deflated}, "signed 16-byte descriptors");
    stored.descriptor = 12; deflated.descriptor = 12;
    good(zip_check_build({stored, deflated}), {stored, deflated}, "unsigned 12-byte descriptors");
    stored.local_sizes = true; deflated.local_sizes = true;
    good(zip_check_build({stored, deflated}), {stored, deflated}, "flag8 with nonzero local sizes");
    good(zip_check_build({stored, deflated}, true), {stored, deflated}, "central directory order differs from local order");
    std::vector<uint8_t> comment{'n','o','t','e','P','K',5,6};
    comment.insert(comment.end(), 24, 255); comment.push_back('x');
    good(zip_check_build({stored, deflated}, false, comment), {stored, deflated}, "EOCD signature inside comment is ignored");

    stored.descriptor = 0; stored.local_sizes = false;
    auto valid = zip_check_build({stored});
    auto wrong = valid;
    zip_check_set32(wrong.bytes, wrong.centrals[0] + 16, 0x12345678);
    bad(wrong, "wrong central CRC rejected");
    wrong = valid; wrong.bytes[wrong.data[0]] ^= 1;
    bad(wrong, "corrupted stored payload CRC rejected");
    wrong = zip_check_build({deflated}); wrong.bytes[wrong.data[0]] = 0xff;
    bad(wrong, "corrupted deflate stream rejected");
    wrong = valid; zip_check_set32(wrong.bytes, wrong.centrals[0] + 24, stored.raw.size() + 1);
    bad(wrong, "uncompressed size disagreement rejected");
    wrong = valid; zip_check_set32(wrong.bytes, wrong.centrals[0] + 20, 0xfffffff0);
    bad(wrong, "compressed payload range overflow rejected");
    wrong = valid; zip_check_set32(wrong.bytes, wrong.centrals[0] + 42, wrong.bytes.size());
    bad(wrong, "local header offset outside archive rejected");
    wrong = valid; zip_check_set32(wrong.bytes, wrong.eocd + 16, 0xfffffff0);
    bad(wrong, "central directory range overflow rejected");
    wrong = valid; wrong.bytes.resize(wrong.bytes.size() - 1);
    bad(wrong, "truncated EOCD rejected");
    wrong = valid; wrong.bytes.resize(wrong.central + 20);
    bad(wrong, "truncated central directory rejected");
    wrong = valid; wrong.bytes.clear();
    bad(wrong, "missing archive headers rejected");
    ZipCheckEntry unsupported = stored; unsupported.method = 99;
    bad(zip_check_build({unsupported}), "unsupported compression method rejected");
    std::cout << "PASS ZipNativeChecks " << assertions << "\n";
}
