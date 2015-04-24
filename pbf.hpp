#ifndef MAPBOX_UTIL_PBF_HPP
#define MAPBOX_UTIL_PBF_HPP

/*
 * Some parts are from upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2008-2011 Google Inc.  See LICENSE for details.
 * Author: Josh Haberman <jhaberman@gmail.com>
 */

#include <cassert>
#include <cstring>
#include <exception>
#include <string>

namespace mapbox { namespace util {

struct pbf {
    struct exception : std::exception { const char *what() const noexcept { return "pbf exception"; } };
    struct unterminated_varint_exception : exception { const char *what() const noexcept { return "pbf unterminated varint exception"; } };
    struct varint_too_long_exception : exception { const char *what() const noexcept { return "pbf varint too long exception"; } };
    struct unknown_field_type_exception : exception { const char *what() const noexcept { return "pbf unknown field type exception"; } };
    struct end_of_buffer_exception : exception { const char *what() const noexcept { return "pbf end of buffer exception"; } };

    inline pbf(const char *data, size_t length);
    inline pbf() = default;

    inline int wire_type() const;
    inline bool is_wire_type(int type) const;

    inline operator bool() const;

    inline bool next();
    inline bool next(uint32_t tag);
    template <typename T = uint32_t> inline T varint();
    template <typename T = uint32_t> inline T svarint();

    template <typename T = uint32_t, int bytes = 4> inline T fixed();
    inline float float32();
    inline double float64();

    inline std::string string();
    inline bool boolean();

    inline pbf message();

    inline void skip();
    inline void skipValue(uint32_t val);
    inline void skipBytes(uint32_t bytes);

    const char *data = nullptr;
    const char *end = nullptr;
    uint32_t value = 0;
    uint32_t tag = 0;
};

pbf::pbf(const char *data_, size_t length)
    : data(data_),
      end(data_ + length),
      value(0),
      tag(0) {
}

int pbf::wire_type() const {
    return value & 0x7;
}

bool pbf::is_wire_type(int type) const {
    return wire_type() == type;
}

pbf::operator bool() const {
    return data < end;
}

bool pbf::next() {
    if (data < end) {
        value = static_cast<uint32_t>(varint());
        tag = value >> 3;
        return true;
    }
    return false;
}

bool pbf::next(uint32_t requested_tag) {
    while (next()) {
        if (tag == requested_tag) {
            return true;
        } else {
            skip();
        }
    }
    return false;
}

template <typename T>
T pbf::varint() {
    assert((is_wire_type(0) || is_wire_type(2)) && "not a varint");
    char byte = static_cast<char>(0x80);
    T result = 0;
    int bitpos;
    for (bitpos = 0; bitpos < 70 && (byte & 0x80); bitpos += 7) {
        if (data >= end) {
            throw unterminated_varint_exception();
        }
        result |= ((T)(byte = *data) & 0x7F) << bitpos;

        data++;
    }
    if (bitpos == 70 && (byte & 0x80)) {
        throw varint_too_long_exception();
    }

    return result;
}

template <typename T>
T pbf::svarint() {
    T n = varint<T>();
    return (n >> 1) ^ -(T)(n & 1);
}

template <typename T, int bytes>
T pbf::fixed() {
    assert(((is_wire_type(5) && bytes == 4) ||
            (is_wire_type(1) && bytes == 8)) && "not a fixed of right size");
    skipBytes(bytes);
    T result;
    memcpy(&result, data - bytes, bytes);
    return result;
}

float pbf::float32() {
    assert(is_wire_type(5) && "not a 32-bit fixed");
    return fixed<float, 4>();
}

double pbf::float64() {
    assert(is_wire_type(1) && "not a 64-bit fixed");
    return fixed<double, 8>();
}

std::string pbf::string() {
    assert(is_wire_type(2) && "not a string");
    uint32_t bytes = static_cast<uint32_t>(varint());
    const char *pos = data;
    skipBytes(bytes);
    return std::string(pos, bytes);
}

bool pbf::boolean() {
    assert(is_wire_type(0) && "not a varint");
    assert((*data & 0x80) == 0 && "not a 1 byte varint");
    skipBytes(1);
    return data[-1] != 0;
}

pbf pbf::message() {
    assert(is_wire_type(2) && "not a message");
    uint32_t bytes = static_cast<uint32_t>(varint());
    const char *pos = data;
    skipBytes(bytes);
    return pbf(pos, bytes);
}

void pbf::skip() {
    skipValue(value);
}

void pbf::skipValue(uint32_t val) {
    switch (val & 0x7) {
        case 0: // varint
            varint();
            break;
        case 1: // 64 bit
            skipBytes(8);
            break;
        case 2: // string/message
            skipBytes(static_cast<uint32_t>(varint()));
            break;
        case 5: // 32 bit
            skipBytes(4);
            break;
        default:
            throw unknown_field_type_exception();
    }
}

void pbf::skipBytes(uint32_t bytes) {
    if (data + bytes > end) {
        throw end_of_buffer_exception();
    }
    data += bytes;
}

}} // end namespace mapbox::util

#endif // MAPBOX_UTIL_PBF_HPP