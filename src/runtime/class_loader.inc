// Class loading, layout and reviewed-library binding boundary.
// Included once by the runtime composition unit; not a standalone TU.
static void layout_class(int id) {
    Class& c = VMS_CL(g_classes)[id];
    int slot = c.super_id >= 0 ? VMS_CL(g_classes)[c.super_id].instance_fields : 0, sslot = 0;
    for (auto& f : c.fields) {
        f.owner = id;
        f.is_static = (f.flags & 0x0008) != 0;
        f.slot = f.is_static ? sslot++ : slot++;
    }
    c.instance_fields = slot;
    c.statics.assign(sslot, Value{});
    for (const auto& f : c.fields) if (f.is_static && (f.desc == "J" || f.desc == "D")) c.statics[f.slot] = Value::of_l(0);
}
static int load_class_bytes(const std::string& name, const std::vector<uint8_t>& data);
static int load_class_file(const std::string& name, bool fatal) {
    if (VMS_CL(g_class_index).count(name)) return VMS_CL(g_class_index)[name];
    for (const auto& dir : VMS_CL(g_classpath)) {
        std::ifstream in(dir + "/" + name + ".class", std::ios::binary);
        if (!in) continue;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
        return load_class_bytes(name, data);
    }
    auto it = VMS_CL(g_jar_files).find(name + ".class");
    if (it != VMS_CL(g_jar_files).end()) return load_class_bytes(name, it->second);
    if (!fatal) return -1;
    std::string have;
    int shown = 0;
    for (auto& kv : VMS_CL(g_jar_files)) {
        if (shown++ >= 8) { have += ",..."; break; }
        if (!have.empty()) have += ",";
        have += kv.first;
    }
    die("class not found: " + name + " jar=[" + have + "]");
    return -1;
}
static int ensure_class(const std::string& name) { return load_class_file(name); }
static int load_class_bytes(const std::string& name, const std::vector<uint8_t>& data) {
    if (VMS_CL(g_class_index).count(name)) return VMS_CL(g_class_index)[name];
    if (data.size() < 10 || ru32(data.data()) != 0xCAFEBABE) die("bad class " + name);
    size_t p = 8;
    auto need = [&](size_t n) { if (p + n > data.size()) die("truncated " + name); };
    auto u1 = [&]() { need(1); return data[p++]; };
    auto u2 = [&]() { need(2); uint16_t v = ru16(&data[p]); p += 2; return v; };
    auto u4 = [&]() { need(4); uint32_t v = ru32(&data[p]); p += 4; return v; };
    int cp_count = u2();
    std::vector<CpEntry> cp(cp_count);
    for (int i = 1; i < cp_count; ++i) {
        CpEntry e; e.tag = u1();
        switch (e.tag) {
        case 7: case 8: case 16: case 19: case 20: e.a = u2(); break;
        case 3: case 4: e.a = (int32_t)u4(); break;
        case 5: case 6: e.l = ((int64_t)u4() << 32) | u4(); break;
        case 9: case 10: case 11: case 12: case 17: case 18: e.a = u2(); e.b = u2(); break;
        case 15: u1(); e.a = u2(); break;
        case 1: {
            int len = u2(); need(len);
            // CONSTANT_Utf8 uses modified UTF-8: normalize its encoded NUL to
            // the same internal byte used by char[] and decoded byte[] strings.
            e.utf.reserve(len);
            for (int n = 0; n < len; ++n) {
                if (data[p+n] == 0xc0 && n+1 < len && data[p+n+1] == 0x80) {
                    e.utf.push_back('\0'); ++n;
                } else e.utf.push_back((char)data[p+n]);
            }
            p += len; break;
        }
        default: die("cp tag " + std::to_string(e.tag) + " in " + name);
        }
        cp[i] = e;
        if (e.tag == 5 || e.tag == 6) ++i;
    }
    uint16_t class_flags = u2();
    int this_idx = u2(), super_idx = u2();
    int ifc = u2();
    std::vector<int> iface_cp(ifc);
    for (int i = 0; i < ifc; ++i) iface_cp[i] = u2();
    std::string this_name = cp[cp[this_idx].a].utf;
    int id = (int)VMS_CL(g_classes).size();
    VMS_CL(g_classes).push_back(Class{});
    VMS_CL(g_class_index)[this_name] = id;
    VMS_CL(g_class_index)[name] = id;
    VMS_CL(g_classes)[id].name = this_name; VMS_CL(g_classes)[id].flags = class_flags;
    VMS_CL(g_classes)[id].cp = std::move(cp);
    if (super_idx) VMS_CL(g_classes)[id].super_id = ensure_class(VMS_CL(g_classes)[id].cp[VMS_CL(g_classes)[id].cp[super_idx].a].utf);
    for (int ix : iface_cp) VMS_CL(g_classes)[id].interfaces.push_back(ensure_class(VMS_CL(g_classes)[id].cp[VMS_CL(g_classes)[id].cp[ix].a].utf));
    int fc = u2();
    for (int i = 0; i < fc; ++i) {
        Field f; f.flags = u2(); f.name = VMS_CL(g_classes)[id].cp[u2()].utf; f.desc = VMS_CL(g_classes)[id].cp[u2()].utf;
        int ac = u2();
        for (int a = 0; a < ac; ++a) {
            int ni = u2(); uint32_t len = u4();
            if (VMS_CL(g_classes)[id].cp[ni].utf == "ConstantValue" && len >= 2) f.const_idx = ru16(&data[p]);
            p += len;
        }
        VMS_CL(g_classes)[id].fields.push_back(f);
    }
    int mc = u2();
    for (int i = 0; i < mc; ++i) {
        Method m; m.flags = u2(); m.name = VMS_CL(g_classes)[id].cp[u2()].utf; m.desc = VMS_CL(g_classes)[id].cp[u2()].utf;
        m.native = (m.flags & 0x0100) != 0; m.class_id = id;
        int ac = u2();
        for (int a = 0; a < ac; ++a) {
            int ni = u2(); uint32_t len = u4();
            if (VMS_CL(g_classes)[id].cp[ni].utf == "Code" && len >= 8) {
                size_t q = p;
                m.max_stack = ru16(&data[q]);
                m.max_locals = ru16(&data[q + 2]);
                uint32_t clen = ru32(&data[q + 4]);
                m.code.assign(data.begin() + q + 8, data.begin() + q + 8 + clen);
                size_t hq = q + 8 + clen;
                if (hq + 2 <= p + len) {
                    int elen = ru16(&data[hq]); hq += 2;
                    for (int e = 0; e < elen && hq + 8 <= p + len; ++e) {
                        Handler h;
                        h.start = ru16(&data[hq]); h.end = ru16(&data[hq + 2]);
                        h.pc = ru16(&data[hq + 4]); h.catch_cp = ru16(&data[hq + 6]);
                        m.handlers.push_back(h); hq += 8;
                    }
                }
            }
            p += len;
        }
        VMS_CL(g_classes)[id].methods.push_back(std::move(m));
    }
    layout_class(id);
    bind_library_layout(id, data);
    Class& c = VMS_CL(g_classes)[id];
    for (auto& f : c.fields) if (f.is_static && f.const_idx > 0 && f.const_idx < (int)c.cp.size()) {
        const CpEntry& e = c.cp[f.const_idx];
        if (e.tag == 3 || e.tag == 4) c.statics[f.slot] = Value::of_i(e.a);
        else if (e.tag == 5 || e.tag == 6) c.statics[f.slot] = Value::of_l(e.l);
        else if (e.tag == 8) c.statics[f.slot] = Value::of_ref(literal_string(cp_utf(c, e.a)));
    }
    return id;
}

