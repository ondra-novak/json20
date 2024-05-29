#pragma once
#include "serialize_common.h"



namespace JSON20_NAMESPACE_NAME {


class serializer_t {
public:


    template<std::invocable<std::string_view> Target>
    constexpr void serialize(const value &v, Target &&target) {

        v.visit([&](const auto &data){
            serialize_item(data, target);
        });

    }


    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary(const value &v, Target &&target) {

        char b = 0;
        target(std::string_view(&b,1)); //store binary mark
        serialize_binary_no_mark(v, std::forward<Target>(target));
    }

    ///serialize to binary, but doesn't store binary mark at the beginning
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_no_mark(const value &v, Target &&target) {

        v.visit([&](const auto &data){
            serialize_binary_item(data, target);
        });

    }




protected:



    std::vector<char> _buffer;

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const std::string_view &str, Target &target) {
        std::size_t needsz = 0;
        for (unsigned char c: str) {
            if (c < 32) [[unlikely]] {
                if (c == '\n' || c == '\b' || c == '\t' || c == '\r' || c == '\f') {
                    ++needsz;
                } else {
                    needsz+=5;
                }
            } else if (c == '"' || c == '\\') {
                ++needsz;
            }

        }
        needsz += str.size();
        _buffer.resize(needsz+2, 0);
        auto wr = _buffer.begin();
        *wr++='"';
        for (unsigned char c: str) {
            if (c < 32) [[unlikely]]{
                *wr++ = '\\';
                switch (c) {
                    case '\n': *wr++ = 'n';break;
                    case '\b': *wr++ = 'b';break;
                    case '\r': *wr++ = 'r';break;
                    case '\f': *wr++ = 'f';break;
                    case '\t': *wr++ = 't';break;
                    default: *wr++ = 'u';
                        for (unsigned int i = 0; i < 4; ++i) {
                            unsigned char x = (c >> ((3-i) * 4)) & 0xF;
                            *wr++ = x + (x>9?'A'-10:'0');
                        }
                }
            } else {
                if (c == '\\' || c == '"') [[unlikely]] {
                    *wr++ = '\\';
                }
                *wr++ = c;
            }
        }
        *wr++='"';
        target(std::string_view(_buffer.data(), _buffer.size()));
    }

    template<bool minus, typename V>
    constexpr char * recurse_number_to_str(V val, int sz = 0) {
        if (val) {
            auto iter = recurse_number_to_str<minus>(val/10, sz+1);
            *iter = (val%10) + '0';
            ++iter;
            return iter;
        }
        if constexpr (minus) {
            _buffer.resize(sz+1,0);
            auto iter = _buffer.data();
            *iter++ = '-';
            return iter;
        } else {
            _buffer.resize(sz,0);
            auto iter = _buffer.data();
            return iter;

        }
    }


    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const double &val, Target &target) {
        constexpr double min_frac_to_render = 0.00001;

        if (is_neg_infinity(val)) {
            serialize_item(number_string::minus_infinity, target);
            return;
        }
        if (is_pos_infinity(val)) {
            serialize_item(number_string::plus_infinity, target);
            return;
        }
        if (is_nan(val)) {
            serialize_item(nullptr,target);
            return;
        }

        double v;
        if (val < 0) {
            target("-");
            v =  -val;
        } else {
            v = val;
        }

        if (v < std::numeric_limits<double>::min()) {
            target("0");
            return;
        }


        int exponent = static_cast<int>(number_string::get_exponent(v));
        if (exponent > 8 || exponent < -2) {
            v = v / number_string::pow10(exponent);
        } else {
            exponent = 0;
        }

        v+=std::numeric_limits<double>::epsilon();

        unsigned long intp = static_cast<unsigned long>(v);
        double fracp = v - intp;
        if (fracp >= (1.0 - min_frac_to_render)) {
            intp++;
            fracp = 0;
        }

        recurse_number_to_str<false>(intp);
        target(std::string_view(_buffer.data(), _buffer.size()));

        if (fracp >= min_frac_to_render) {
            _buffer.clear();
            _buffer.push_back('.');
            while (fracp >= min_frac_to_render) {
                fracp *= 10;
                unsigned int vv = static_cast<int>(fracp);
                fracp -= vv;
                if (fracp >= (1.0-min_frac_to_render) && vv < 9) {
                    vv++;
                    _buffer.push_back(static_cast<char>(vv) + '0');
                    break;
                }
                _buffer.push_back(static_cast<char>(vv) + '0');
            }
            target(std::string_view(_buffer.data(), _buffer.size()));
        }
        if (exponent) {
            target("e");
            serialize_item(exponent, target);
        }

    }

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const bool &v, Target &target) {
        target(v?"true":"false");
    }


    template<signed_integer_number_t V, std::invocable<std::string_view> Target>
    constexpr void serialize_item(const V &v, Target &target) {
        if (v < 0) {
            auto end = recurse_number_to_str<true>(static_cast<std::make_unsigned_t<V> >(-v));
            target(std::string_view(_buffer.data(), end));
        } else if (v>0) {
            auto end = recurse_number_to_str<false>(static_cast<std::make_unsigned_t<V> >(v));
            target(std::string_view(_buffer.data(), end));
        } else {
            target("0");
        }
    }


    template<unsigned_integer_number_t V, std::invocable<std::string_view> Target>
    constexpr void serialize_item(const V &v, Target &target) {
        if (v == 0) target("0");
        else {
            auto end = recurse_number_to_str<false>(v);
            target(std::string_view(_buffer.data(), end));
        }
    }

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const number_string &v, Target &target) {
        target(std::string_view(v));
    }

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const std::nullptr_t &, Target &target) {
        target("null");
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const undefined_t &, Target &target) {
        target("null");
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const placeholder_view &v, Target &target) {
        target("\"${");
        serialize_item(v.position, target);
        target("}\"");
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const binary_string_view_t &data, Target &target) {
        _buffer.resize((data.size()+2)/3*4+2,0);
        auto wr = _buffer.begin();
        *wr++='"';
        if (!data.empty()) {
            wr = base64.encode(data.begin(), data.end(), wr);
        }
        *wr++='"';
        target(std::string_view(_buffer.begin(), wr));
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const array_view &data, Target &target) {
        if (data.empty()) target("[]");
        else {
            target("[");
            auto iter = data.begin();
            serialize(*iter, target);
            ++iter;
            while (iter != data.end()) {
                target(",");
                serialize(*iter, target);
                ++iter;
            }
            target("]");
        }
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_item(const object_view &data, Target &target) {
        if (data.empty()) target("{}");
        else {
            target("{");
            auto iter = data.begin();
            serialize_item(iter->key, target);
            target(":");
            serialize(iter->value, target);
            ++iter;
            while (iter != data.end()) {
                target(",");
                serialize_item(iter->key, target);
                target(":");
                serialize(iter->value, target);
                ++iter;
            }
            target("}");
        }

    }

    template<std::invocable<std::string_view> Target, unsigned_integer_number_t Num>
    static void make_tlv_tag(bin_element_t tag, Num l,Target &target) {
        if (l < static_cast<Num>(8)) {
            char b = static_cast<char>(tag) | static_cast<char>(l);
            target(std::string_view(&b,1));
        } else {
            char buff[10];
            char *p = buff;
            auto c = l;
            unsigned char b = 0;
            while ((c >>= 8)) ++b;
            *p = static_cast<char>(tag) | static_cast<char>(b+8);
            ++p;
            for (unsigned char i = 0; i <= b; ++i) {
                *p++ = static_cast<char>(l & 0xFF);
                l >>=8;
            }
            target(std::string_view(buff, p-buff));
        }
    }


    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const std::string_view &str, Target &target) {
        make_tlv_tag(bin_element_t::string, str.size(), target);
        target(str);
    }

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const bool &v, Target &target) {
        char b = static_cast<char>(v?bin_element_t::bool_true:bin_element_t::bool_false);
        target(std::string_view(&b,1));
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const double &v, Target &target) {
        std::uint64_t bin_val = std::bit_cast<std::uint64_t>(v);
        char data[10];
        char *p = data;
        *p++ = static_cast<char>(bin_element_t::num_double);
        for (unsigned int i = 0; i < 8; ++i) {
            *p++ = static_cast<char>(bin_val & 0xFF);
            bin_val >>=8;
        }
        target(std::string_view(data, p - data));
    }

    template<signed_integer_number_t V, std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const V &v, Target &target) {
        if (v < 0) {
            make_tlv_tag(bin_element_t::neg_number, static_cast<std::make_unsigned_t<V> >(-v), target);
        } else {
            make_tlv_tag(bin_element_t::pos_number, static_cast<std::make_unsigned_t<V> >(v), target);
        }
    }


    template<unsigned_integer_number_t V, std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const V &v, Target &target) {
        make_tlv_tag(bin_element_t::pos_number, v, target);
    }

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const number_string &v, Target &target) {
        make_tlv_tag(bin_element_t::num_string, v.size(), target);
        target(v);
    }

    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const placeholder_view &plc, Target &target) {
        char buff[3];
        buff[0] = static_cast<char>(bin_element_t::placeholder);;
        buff[1] = plc.position & 0xFF;
        buff[2] = (plc.position>>8) & 0xFF;
        target(std::string_view(buff,3));

    }


    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const std::nullptr_t &, Target &target) {
        char b = static_cast<char>(bin_element_t::null);
        target(std::string_view(&b,1));
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const undefined_t &, Target &target) {
        char b = static_cast<char>(bin_element_t::undefined);
        target(std::string_view(&b,1));
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const binary_string_view_t &data, Target &target) {
        make_tlv_tag(bin_element_t::bin_string, data.size(), target);
        _buffer.resize(data.size(),0);
        std::copy(data.begin(), data.end(), _buffer.begin());
        target(std::string_view(_buffer.data(), _buffer.size()));
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const array_view &data, Target &target) {
        make_tlv_tag(bin_element_t::array, data.size(), target);
        for (const value &v: data) {
            serialize_binary_no_mark(v, target);
        }
    }
    template<std::invocable<std::string_view> Target>
    constexpr void serialize_binary_item(const object_view &data, Target &target) {
        make_tlv_tag(bin_element_t::object, data.size(), target);
        for (auto iter = data.begin(); iter != data.end(); ++iter) {
            serialize_binary_item(iter->key, target);
            serialize_binary_no_mark(iter->value, target);
        }
    }



};

inline std::string value::to_json() const {
    std::string res;
    serializer_t srl;
    srl.serialize(*this, [&](const std::string_view &a){
        res.append(a);
    });
    return res;
}

inline constexpr std::string_view value::to_json(std::vector<char> &buffer) const {
    std::size_t start = buffer.size();
    serializer_t srl;
    srl.serialize(*this, [&](const std::string_view &a){
        buffer.insert(buffer.end(), a.begin(), a.end());
    });
    return std::string_view(buffer.data()+start, buffer.size() - start);
}

template<typename Fn>
class to_json_string_t: public std::string_view {
public:

    constexpr to_json_string_t(Fn fn):std::string_view(buffer,size) {
        char *x = buffer;
        serializer_t srl;
        srl.serialize(fn(), [&](std::string_view y){
            x = std::copy(y.begin(), y.end(), x);
        });
    }

protected:
    static constexpr Fn _dummy = {};
    static constexpr std::size_t size = ([]{
         std::vector<char> buff;
         return _dummy().to_json(buff).size();
    })();

    char buffer[size] = {};
};

template<typename Fn>
constexpr to_json_string_t<Fn> to_json_string(Fn fn) {
    return to_json_string_t<Fn>(fn);
}

}
