#pragma once
#include "value.h"
#include <vector>
#include <iterator>

namespace json20 {



class serializer_t {
public:


    constexpr serializer_t(const value_t &val) {
        _stack.push_back({
            Type::value, &val, &val+1
        });
    }

    constexpr std::string_view read();


    template<typename Iter>
    static constexpr void encode_str(std::string_view str, Iter iter);


protected:


    constexpr void render_item(const value_t &item);
    constexpr void close_item();
    constexpr void render(const array_t &arr);
    constexpr void render(const object_t &arr);
    constexpr void render(const std::string_view &str);
    constexpr void render(const number_string_t &str);
    constexpr void render(const undefined_t &);
    constexpr void render(const std::nullptr_t &);
    constexpr void render(const bool &);
    template<signed_integer_number_t T>
    constexpr void render(const T &val) {
        if (val < 0) {
            _buffer.push_back('-');
            render(static_cast<std::make_unsigned_t<T> >(-val));
        } else{
            render(static_cast<std::make_unsigned_t<T> >(val));
        }
    }
    template<unsigned_integer_number_t T>
    constexpr void render(const T &val) {
        if (val == 0) {
            _buffer.push_back('0');
            return;
        }
        T v = val;
         char buff[100];
         auto iter = std::end(buff);
         while (v) {
             --iter;
             *iter = ((v % 10) + '0');
             v /= 10;
         }
         std::copy(iter, std::end(buff), std::back_inserter(_buffer));
     }

    constexpr void render(const double &);


    constexpr void render_key(const std::string_view &key);
    constexpr void render_kw(const std::string_view &kw);

    enum Type {
        value,
        object,
        array,
        key
    };

    struct state_t {
        Type type;
        value_t::iterator_t iter;
        value_t::iterator_t end;
    };


    std::vector<state_t> _stack;
    std::vector<char> _buffer;
};




inline constexpr std::string_view serializer_t::read() {

    _buffer.clear();
    if (_stack.empty()) return {};
    auto &top = _stack.back();
    bool has_key = top.type == Type::object;
    if (has_key) {
        auto iter = top.iter++;
        _stack.push_back({
            Type::value, iter, iter+1
        });
        render_key(iter.key());
    } else {
        const auto &item = *top.iter;
        ++top.iter;
        auto stksz = _stack.size();
        render_item(item);
        if (stksz == _stack.size()) {
            close_item();
        }
    }
    return {_buffer.data(), _buffer.size()};
}


inline constexpr void serializer_t::render_item(const value_t &item) {
    item.visit([&](const auto &v){
        render(v);
    });
}

inline constexpr void serializer_t::close_item() {
    do {
        auto &top = _stack.back();
        if (top.iter == top.end) {
            switch (top.type) {
                default: break;
                case Type::array: _buffer.push_back(']');break;
                case Type::object: _buffer.push_back('}');break;
            }
            _stack.pop_back();
            if (_stack.empty()) return;
        } else {
            _buffer.push_back(',');
            return;
        }
    } while (true);
}

inline constexpr void serializer_t::render(const array_t &arr) {
    _buffer.push_back('[');
    _stack.push_back({
        Type::array,
        value_t::iterator_t(arr.data()),
        value_t::iterator_t(arr.data()+arr.size())
    });
}

inline constexpr void serializer_t::render(const object_t &obj) {
    _buffer.push_back('{');
    _stack.push_back({
        Type::object,
        value_t::iterator_t(obj.data()),
        value_t::iterator_t(obj.data()+obj.size())
    });
}



inline constexpr void serializer_t::render_key(const std::string_view &key) {
    render(key);
    _buffer.push_back(':');
}

template<typename Iter>
constexpr void serializer_t::encode_str(std::string_view text, Iter iter) {

    auto escape = [&](char c) {
        *iter = '\\'; ++iter;
        *iter = c; ++iter;
    };

    auto hex = [](int val) {
        return static_cast<char>(val <= 10?('0'+val):('A'+val-10));
    };

    for (char c : text) {
          switch (c) {
              case '"': escape('"');break;
              case '\\':escape('\\');break;
              case '\b':escape('b');break;
              case '\f':escape('f');break;
              case '\n':escape('n');break;
              case '\r':escape('r');break;
              case '\t':escape('t');break;
              default:
                  if (c>= 0 && c < 0x20) {
                      escape('u');
                      unsigned int val = c;
                      *iter = '0'; ++iter;
                      *iter = '0'; ++iter;
                      *iter = hex(val >> 4);++iter;
                      *iter = hex(val & 0xF);++iter;
                  } else {
                      *iter = c; ++iter;
                  }
                  break;
          }
    }

}



inline constexpr void serializer_t::render(const std::string_view &str) {
    _buffer.push_back('"');
    encode_str(str, std::back_inserter(_buffer));
    _buffer.push_back('"');
}

inline constexpr void serializer_t::render(const number_string_t &str) {
    encode_str(str, std::back_inserter(_buffer));
}
inline constexpr void serializer_t::render_kw(const std::string_view &kw) {
    std::copy(kw.begin(), kw.end(), std::back_inserter(_buffer));
}

inline constexpr void serializer_t::render(const undefined_t &) {
    render_kw(value_t::str_null);
}
inline constexpr void serializer_t::render(const std::nullptr_t &) {
    render_kw(value_t::str_null);
}
inline constexpr void serializer_t::render(const bool &b) {
    render_kw(b?value_t::str_true:value_t::str_false);
}
inline std::string value_t::stringify() const {
    std::string res;
    serializer_t srl(*this);
    std::string_view str;
    while (!(str = srl.read()).empty()) res.append(str);
    return res;
}

inline constexpr void serializer_t::render(const double & val) {

    constexpr double min_frac_to_render = 0.0001;

    if (is_neg_infinity(val)) {
        render(number_string_t::minus_infinity);
        return;
    }
    if (is_pos_infinity(val)) {
        render(number_string_t::plus_infinity);
        return;
    }
    if (is_nan(val)) {
        render(nullptr);
        return;
    }

    double v;
    if (val < 0) {
        _buffer.push_back('-');
        v =  -val;
    } else {
        v = val;
    }


    int exponent = static_cast<int>(number_string_t::get_exponent(v));
    if (exponent > 8 || exponent < -2) {
        v = v / number_string_t::pow10(exponent);
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
    render(intp);
    if (fracp >= min_frac_to_render) {
        _buffer.push_back('.');
        while (fracp >= min_frac_to_render && fracp <= (1.0-min_frac_to_render)) {
            fracp *= 10;
            unsigned int vv = static_cast<int>(fracp);
            fracp -= vv;
            _buffer.push_back(static_cast<char>(vv) + '0');
        }
    }
    if (exponent) {
        _buffer.push_back('e');
        render(exponent);
    }

}



}
