// Snapshots contain only text, never Java references or pointers to live frames.
// Keeping them outside Obj avoids increasing every ordinary object's size.
 VMS_DIAG_ACCESSOR(g_exception_traces, std::unordered_map<int, std::string>)
static void capture_exception_trace(int ref) {
    std::string trace;
    size_t count = 0;
    bool constructors = true;
    std::unordered_set<const std::vector<Frame>*> seen;
    for (auto scope = VMS_HEAP(g_root_stacks).rbegin(); scope != VMS_HEAP(g_root_stacks).rend(); ++scope) {
        if (scope->second != &current_context() || !seen.insert(scope->first).second) continue;
        for (auto frame = scope->first->rbegin(); frame != scope->first->rend(); ++frame) {
            const Method* m = frame->method;
            if (!m) continue;
            if (constructors && m->name == "<init>" && !frame->locals.empty() && frame->locals[0].ref == ref) continue;
            constructors = false;
            if (count++ == 128) { trace += "\n\t... trace truncated at 128 frames"; VMS_DIAG(g_exception_traces)[ref] = std::move(trace); return; }
            std::string owner = VMS_CL(g_classes)[m->class_id].name;
            std::replace(owner.begin(), owner.end(), '/', '.');
            trace += "\n\tat " + owner + "." + m->name + m->desc + " (bci=" + std::to_string(frame->op_pc) + ")";
        }
    }
    VMS_DIAG(g_exception_traces)[ref] = std::move(trace);
}
static void log_exception_trace(int ref) {
    auto it = VMS_DIAG(g_exception_traces).find(ref);
    if (it == VMS_DIAG(g_exception_traces).end()) return;
    size_t pos = 0;
    while (pos < it->second.size()) {
        if (it->second[pos] == '\n') ++pos;
        size_t end = it->second.find('\n', pos);
        if (end == std::string::npos) end = it->second.size();
        log_line("EX " + it->second.substr(pos, end-pos)); pos = end;
    }
}
static void flush_console(int ref, bool newline) {
    std::string& pending = VMS_HEAP(g_heap)[ref].text;
    if (newline || !pending.empty()) {
        if (!pending.empty() && pending.back() == '\r') pending.pop_back();
        log_line("JAVA " + pending);
        if (pending.find_first_not_of(" \t\r\n") != std::string::npos) VMS_EXEC(g_last_console_line) = pending;  // 0.2.112 F2
        pending.clear();
    }
}
