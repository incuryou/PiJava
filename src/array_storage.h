#ifndef J2ME_VM_ARRAY_STORAGE_H
#define J2ME_VM_ARRAY_STORAGE_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "resource_version.h"

// The including translation unit must define Value before including this file.
// Each Java element occupies only its JVM storage width. Floating-point values
// use Value's integer fields, preserving their raw bits without conversion.
class ArrayStorage {
public:
    bool reset(const std::string& descriptor, std::size_t length) {
        char kind = 0;
        std::size_t width = 0;
        if (!parse_descriptor(descriptor, kind, width) ||
            length > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()) ||
            length > std::numeric_limits<std::size_t>::max() / width ||
            length > bytes_.max_size() / width) {
            return false;
        }
        // Invalid requests leave the old contents intact. As with std::vector
        // elsewhere in the VM, allocation failure is fatal with -fno-exceptions.
        std::vector<uint8_t> replacement(length * width, uint8_t{0});
        bytes_.swap(replacement);
        length_ = static_cast<uint32_t>(length);
        width_ = static_cast<uint8_t>(width);
        kind_ = kind;
        ++reference_version_; last_reference_write_=SIZE_MAX;
        layout_version_ = content_version_ = resver::tick(); ++resver::g_stats.array_layout;
        return true;
    }

    std::size_t size() const { return length_; }
    bool empty() const { return length_ == 0; }
    bool references() const { return kind_ == 'R'; }
    uint64_t reference_version() const { return reference_version_; }
    size_t last_reference_write() const { return last_reference_write_; }
    std::size_t payload_bytes() const { return bytes_.size(); }
    std::size_t capacity_bytes() const { return bytes_.capacity(); }

    Value get(std::size_t index) const {
        assert(index < length_);
        const uint8_t* element = bytes_.data() + index * width_;
        switch (kind_) {
        case 'Z': return Value::of_i(element[0] & 1u);
        case 'B': {
            const int32_t bits = element[0];
            return Value::of_i(bits < 128 ? bits : bits - 256);
        }
        case 'C': return Value::of_i(load<uint16_t>(element));
        case 'S': {
            const int32_t bits = load<uint16_t>(element);
            return Value::of_i(bits < 32768 ? bits : bits - 65536);
        }
        case 'I':
        case 'F': return Value::of_i(load<int32_t>(element));
        case 'J':
        case 'D': return Value::of_l(load<int64_t>(element));
        case 'R': return Value::of_ref(load<int32_t>(element));
        default: assert(false); return Value::of_i(0);
        }
    }

    void set(std::size_t index, Value value) {
        assert(index < length_);
        content_version_ = resver::tick(); ++resver::g_stats.array_content; // P2.5 dirty barrier
        uint8_t* element = bytes_.data() + index * width_;
        switch (kind_) {
        case 'Z': element[0] = static_cast<uint8_t>(value.i) & 1u; break;
        case 'B': element[0] = static_cast<uint8_t>(value.i); break;
        case 'C':
        case 'S': store(element, static_cast<uint16_t>(value.i)); break;
        case 'I':
        case 'F': store(element, value.i); break;
        case 'J':
        case 'D': store(element, value.l); break;
        case 'R': { const int32_t ref=value.ref; store(element, ref); ++reference_version_; last_reference_write_=index; break; }
        default: assert(false); break;
        }
    }

    // The caller performs Java reference assignability checks. Reference array
    // descriptors share the same storage kind; primitive kinds must match.
    bool copy_from(const ArrayStorage& source, std::size_t source_index,
                   std::size_t destination_index, std::size_t count) {
        if (kind_ == 0 || kind_ != source.kind_ ||
            source_index > source.length_ || destination_index > length_ ||
            count > source.length_ - source_index ||
            count > length_ - destination_index) {
            return false;
        }
        if (count != 0) {
            std::memmove(bytes_.data() + destination_index * width_,
                         source.bytes_.data() + source_index * width_,
                         count * width_);
            content_version_ = resver::tick(); ++resver::g_stats.array_content; // P2.5 dirty barrier
            if(references()){++reference_version_;last_reference_write_=SIZE_MAX;}
        }
        return true;
    }

    const uint8_t* byte_data() const {
        return kind_ == 'B' || kind_ == 'Z' ? bytes_.data() : nullptr;
    }
    // P2.5: the mutable byte view IS a write barrier (short borrow: write immediately, keep nothing).
    // Read-only callers use the const overload / read_bytes().
    uint8_t* byte_data() {
        if (kind_ != 'B' && kind_ != 'Z') return nullptr;
        content_version_ = resver::tick(); ++resver::g_stats.array_view;
        return bytes_.data();
    }
    const uint8_t* read_bytes() const { return byte_data(); }
    uint64_t layout_version() const { return layout_version_; }
    uint64_t content_version() const { return content_version_; }
    // Raw payload for verification hashing only (read-only).
    const uint8_t* raw_payload() const { return bytes_.data(); }
    const int32_t* int_data() const {
        return kind_ == 'I' ? reinterpret_cast<const int32_t*>(bytes_.data()) : nullptr;
    }

private:
    uint64_t reference_version_ = 0;
    uint64_t layout_version_ = 0, content_version_ = 0;
    size_t last_reference_write_ = SIZE_MAX;
    template <typename T>
    static T load(const uint8_t* source) {
        T result;
        std::memcpy(&result, source, sizeof(result));
        return result;
    }

    template <typename T>
    static void store(uint8_t* destination, T value) {
        std::memcpy(destination, &value, sizeof(value));
    }

    static bool parse_descriptor(const std::string& descriptor, char& kind,
                                 std::size_t& width) {
        if (descriptor.size() < 2 || descriptor[0] != '[') return false;
        std::size_t dimensions = 0;
        while (dimensions < descriptor.size() && descriptor[dimensions] == '[') {
            ++dimensions;
        }
        if (dimensions > 255 || dimensions == descriptor.size()) return false;
        const char leaf = descriptor[dimensions];
        if (leaf == 'L') {
            if (descriptor.size() < dimensions + 3 || descriptor.back() != ';') {
                return false;
            }
            // A nonempty internal class name consists of nonempty slash-separated
            // names, with no descriptor punctuation, dots, or embedded NULs.
            bool segment_empty = true;
            for (std::size_t i = dimensions + 1; i + 1 < descriptor.size(); ++i) {
                const char c = descriptor[i];
                if (c == '.' || c == ';' || c == '[' || c == '\0') return false;
                if (c == '/') {
                    if (segment_empty) return false;
                    segment_empty = true;
                } else {
                    segment_empty = false;
                }
            }
            if (segment_empty) return false;
        } else {
            if (dimensions + 1 != descriptor.size()) return false;
            switch (leaf) {
            case 'Z': case 'B': case 'C': case 'S':
            case 'I': case 'F': case 'J': case 'D': break;
            default: return false;
            }
        }

        kind = dimensions > 1 || leaf == 'L' ? 'R' : leaf;
        switch (kind) {
        case 'Z': case 'B': width = 1; break;
        case 'C': case 'S': width = 2; break;
        case 'I': case 'F': case 'R': width = 4; break;
        case 'J': case 'D': width = 8; break;
        default: return false;
        }
        return true;
    }

    std::vector<uint8_t> bytes_;
    uint32_t length_ = 0;
    uint8_t width_ = 0;
    char kind_ = 0;
};

#endif // J2ME_VM_ARRAY_STORAGE_H
