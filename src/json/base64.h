#pragma once

#include "common.h"

namespace JSON20_NAMESPACE_NAME {

class base64_t {
public:

    constexpr base64_t(const char *charset, char terminator):_terminator(terminator) {
        std::copy(charset,charset+64, _charset);
        std::fill(std::begin(_charmap), std::end(_charmap), static_cast<char>(-1));
        for (unsigned int i = 0; i < 64;++i) {
            int c = _charset[i]-32;
            _charmap[c] = static_cast<char>(i);
        }
    }

    template<typename InIter, typename OutIter>
    constexpr OutIter encode(InIter beg, InIter end, OutIter out) const {
        unsigned int remain = 0;
        unsigned int accum = 0;
        while (beg != end) {
            accum =  static_cast<unsigned char>(*beg);
            ++beg;
            if (beg == end) {
                remain = 2;
                break;
            }
            accum = (accum << 8) | static_cast<unsigned char>(*beg);
            ++beg;
            if (beg == end) {
                remain = 1;
                break;
            }
            accum = (accum << 8) | static_cast<unsigned char>(*beg);
            ++beg;
            *out = _charset[accum >> 18];
            ++out;
            *out = _charset[(accum >> 12) & 0x3F];
            ++out;
            *out = _charset[(accum >> 6) & 0x3F];
            ++out;
            *out = _charset[accum & 0x3F];
            ++out;
            accum = 0;
        }
        switch (remain) {
            case 2: *out = _charset[accum >> 2];
                    ++out;
                    *out = _charset[(accum << 4) & 0x3F];
                    ++out;
                    if (_terminator) {
                        *out = _terminator;
                        ++out;
                        *out = _terminator;
                        ++out;
                    }
                    break;
            case 1: *out = _charset[accum >> 10];
                    ++out;
                    *out = _charset[(accum >> 4) & 0x3F];
                    ++out;
                    *out = _charset[(accum << 2) & 0x3F];
                    ++out;
                    if (_terminator) {
                        *out = _terminator;
                        ++out;
                    }
                    break;
            default:
                    break;
        }
        return out;
    }
    template<typename InIter, typename OutIter>
    constexpr OutIter decode(InIter beg, InIter end, OutIter out) const {
        unsigned int accum = 0;
        unsigned int remain = 0;
        char c;

        auto load_next = [&]() {
            do {
                if (beg == end || *beg == _terminator) return false;
                auto val = static_cast<unsigned int>(*beg)-32;
                ++beg;
                if (val < 96) {
                    c = _charmap[val];
                    if (c != -1) return true;
                }
                //skip invalid characters
            } while (true);
        };

        while (load_next()) {
            accum = c;
            if (!load_next()) {
                remain = 3;
                break;
            }
            accum  = (accum << 6) | c;
            if (!load_next()) {
                remain = 2;
                break;
            }
            accum  = (accum << 6) | c;
            if (!load_next()) {
                remain = 1;
                break;
            }
            accum  = (accum << 6) | c;
            *out = static_cast<char>(accum >> 16);
            ++out;
            *out = static_cast<char>((accum >> 8) & 0xFF);
            ++out;
            *out = static_cast<char>(accum & 0xFF);
            ++out;
        }
        switch (remain) {
            default: break;
            case 2: *out = static_cast<char>(accum >> 4);
                    ++out;
                     break;
            case 1:*out = static_cast<char>(accum >> 10);
                    ++out;
                    *out = static_cast<char>((accum >> 2) & 0xFF);
                    ++out;
                    break;
        }
        return out;
    }


protected:
    char _charset[64];
    char _charmap[96] = {};
    char _terminator;

};

inline constexpr base64_t base64 = {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",'='};
inline constexpr base64_t base64url = {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",'\0'};

template<std::size_t N>
class binary_data {
public:

    constexpr binary_data(std::string_view txt) {
        auto res =base64.decode(txt.begin(), txt.end(), data);
        sz = res - data;
    }

    constexpr operator std::basic_string_view<unsigned char>() const {
        return {data, sz};
    }


    constexpr auto begin() const {return data;}
    constexpr auto end() const {return data+sz;}
    constexpr std::size_t size() const {return sz;}

protected:
    std::size_t sz;
    unsigned char data[N]= {};
};

template<std::size_t N>
binary_data(const char (&)[N]) -> binary_data<(N+3)*3/4>;



}
