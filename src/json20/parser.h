#pragma once
#include <locale>
#include <string_view>
#include <vector>
#include <string>
#include "serialize_common.h"


namespace json20 {


template<format_t format  = format_t::text>
class parser_t {
public:

    constexpr parser_t() {reset();}

    constexpr bool write(const std::string_view &text);

    constexpr const value_t &get_parsed() const {
        return _result;
    }

    constexpr std::string_view get_unused_text() const {
        return std::string_view(text_begin, text_end);
    }

    constexpr void reset();

    constexpr bool is_error() const {return _parse_error || !_is_done;}

    constexpr bool is_done() const {return _is_done;}

protected:

    enum class state_type_t {
        read_until,
        read_kw,
        number,
        string,
        array,
        array_begin,
        object,
        object_begin,
        detect,
        skip_ws,
        bin_number,
        bin_neg_number,
        bin_double,
        bin_string,
        num_string,
    };

    struct text_state_t {
        state_type_t st;
        bool flag = false;
        std::string_view str = {};
        value_t key = {};
        std::vector<value_t> arr = {};
    };

    struct binary_state_t {
        state_type_t st;
        std::uint64_t number = 0;
        unsigned int byte_count = 0;
        value_t key = {};
        std::vector<value_t> arr = {};
    };

    using state_t = std::conditional_t<format == format_t::text, text_state_t, binary_state_t>;

    value_t _result;
    std::vector<char> _buffer;
    std::vector<char> _decoded_buffer;
    std::vector<state_t> _stack;
    const char *text_begin = 0;
    const char *text_end = 0;
    const char *text_parse_begin = 0;
    bool eof = false;
    bool _parse_error = false;
    bool _is_done = false;

    constexpr bool set_parse_error() {
        _parse_error = true;
        return true;
    }

    static constexpr std::string_view white_characters = " \t\r\n";
    static constexpr bool is_whitespace(char c) {
        return white_characters.find(c) != white_characters.npos;
    }

    constexpr bool parse();
    constexpr bool read_text_until(const std::string_view stop_chars, bool &flag);
    constexpr bool parse_top();
    constexpr bool detect_value();
    template<state_type_t type>
    constexpr bool parse_string(state_t &action);
    constexpr bool parse_number(state_t &action);
    constexpr bool parse_kw(state_t &action);
    constexpr bool parse_object(state_t &action);
    constexpr bool parse_object_begin(state_t &action);
    constexpr bool parse_array(state_t &action);
    constexpr bool parse_array_begin(state_t &action);
    constexpr bool skip_ws();
    constexpr bool parse_bin_number(state_t &action);
    constexpr bool parse_bin_neg_number(state_t &action);
    constexpr bool parse_bin_double(state_t &action);

    static constexpr value_t alloc_str(std::string_view s);
    static constexpr value_t alloc_num_str(std::string_view s);
    static constexpr value_t alloc_bin_str(std::string_view s);
    template<typename Iter, typename OutIter>
    static constexpr OutIter decode_json_string(Iter beg, Iter end, OutIter output);
    static constexpr int hex_to_int(char hex);


    constexpr value_t make_object(std::vector<value_t> &keypairs) {
        shared_array_t<key_value_t> *kvarr = shared_array_t<key_value_t>::create(keypairs.size()/2,[&](auto *beg, auto *end){
            auto iter = keypairs.begin();
           while (beg != end) {
               beg->key = std::move(*iter);
               ++iter;
               beg->value = std::move(*iter);
               ++iter;
               ++beg;

           }
        });
        return value_t(kvarr);
    }
};


template<format_t format>
constexpr int parser_t<format>::hex_to_int(char hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        return hex - 'a' + 10;
    }
    return 0;
}

template<format_t format>
template<typename Iter, typename OutIter>
constexpr OutIter parser_t<format>::decode_json_string(Iter beg, Iter end, OutIter output) {
    for (auto it = beg; it != end; ++it) {
        if (*it == '\\') {
            ++it;
            if (it == end) return output;
            char c = *it;
            switch (c) {
                case '"':
                case '\\':
                case '/':
                    *output = c;
                    break;
                case 'b':
                    *output = '\b';
                    break;
                case 'f':
                    *output = '\f';
                    break;
                case 'n':
                    *output = '\n';
                    break;
                case 'r':
                    *output = '\r';
                    break;
                case 't':
                    *output = '\t';
                    break;
                case 'u': {
                    ++it;
                    int codepoint = 0;
                    for (int i = 0; i < 4 && it != end; ++i) {
                        codepoint = (codepoint << 4) | hex_to_int(*it);
                        ++it;
                    }
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        if (it == end || *it != '\\') return output;
                        ++it;
                        if (it == end || *it != 'u') return output;
                        ++it;
                        int second_codepoint = 0;
                        for (int i = 0; i < 4 && it != end; ++i) {
                            second_codepoint = (second_codepoint << 4) | hex_to_int(*it);
                            ++it;
                        }
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (second_codepoint - 0xDC00);
                    }
                    --it;
                    if (codepoint <= 0x7F) {
                        *output = static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        *output++ = static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                        *output = static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0xFFFF) {
                        *output++ = static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                        *output++ = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        *output = static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0x10FFFF) {
                        *output++ = static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
                        *output++ = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        *output++ = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        *output = static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                }
                    break;
                default:
                    break;
            }
        } else {
            *output = *it;
        }
        ++output;
    }
    return output;
}


template<format_t format>
inline constexpr bool parser_t<format>::write(const std::string_view &text) {
    eof = text.empty();
    text_begin = text.data();
    text_end = text.data()+text.size();
    if (_parse_error || parse()) {
        _is_done = true;
        return true;
    }
    while (text_begin != text_end) {
        if (_parse_error || parse()) {
            _is_done = true;
            return true;
        }
    }
    return _parse_error;
}

template<format_t format>
inline constexpr bool parser_t<format>::read_text_until(const std::string_view stop_chars, bool &flag) {

    if (eof) return true;
    text_parse_begin = text_begin;

    while (text_begin != text_end) {
        char c= *text_begin;
        if (!flag && stop_chars.find(c) != stop_chars.npos) {
            return true;
        }
        ++text_begin;
        _buffer.push_back(c);
        flag = c == '\\';
    }
    return false;

}
template<format_t format>
inline constexpr void parser_t<format>::reset() {
    _stack.push_back({state_type_t::detect});
    _is_done = false;
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_top() {
    auto &top = _stack.back();
    switch (top.st) {
        case state_type_t::detect:
            return detect_value();
        case state_type_t::array_begin:
            return parse_array_begin(top);
        case state_type_t::array:
            return parse_array(top);
        case state_type_t::number:
            return parse_number(top);
        case state_type_t::read_kw:
            return parse_kw(top);
        case state_type_t::read_until:
            if constexpr(format == format_t::text) {
                return read_text_until(top.str, top.flag);
            }
            return true;
        case state_type_t::string:
            return parse_string<state_type_t::string>(top);
        case state_type_t::object:
            return parse_object(top);
        case state_type_t::object_begin:
            return parse_object_begin(top);
        case state_type_t::skip_ws:
            return skip_ws();
        case state_type_t::bin_number:
            return parse_bin_number(top);
        case state_type_t::bin_neg_number:
            return parse_bin_neg_number(top);
        case state_type_t::bin_double:
            return parse_bin_double(top);
        case state_type_t::bin_string:
            return parse_string<state_type_t::bin_string>(top);
        case state_type_t::num_string:
            return parse_string<state_type_t::num_string>(top);
    }
}


template<format_t format>
inline constexpr bool parser_t<format>::parse() {
    while (!_stack.empty()) {
        if (!parse_top()) return false;
        _stack.pop_back();
    }
    return true;
}

template<format_t format>
inline constexpr bool parser_t<format>::detect_value() {
    if (eof) return set_parse_error();
    if constexpr(format == format_t::text) {
        if (skip_ws()) {
            char c = *text_begin;
            ++text_begin;
            _stack.pop_back();
            switch (c) {
                case '[': _stack.push_back({state_type_t::array_begin});
                          break;
                case '{': _stack.push_back({state_type_t::object_begin});
                          break;
                case '"': _stack.push_back({state_type_t::string});
                          _stack.push_back({state_type_t::read_until, false, "\""});
                          break;
                case value_t::str_true[0]: _stack.push_back({state_type_t::read_kw, false, value_t::str_true, true});
                          --text_begin;
                          break;
                case value_t::str_false[0]: _stack.push_back({state_type_t::read_kw, false, value_t::str_false, false});
                          --text_begin;
                          break;
                case value_t::str_null[0]: _stack.push_back({state_type_t::read_kw, false, value_t::str_null, nullptr});
                          --text_begin;
                          break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '-':
                case '+': _stack.push_back({state_type_t::number});
                          _stack.push_back({state_type_t::read_until, false, ",}] \t\r\n"});
                          --text_begin;
                          break;
                default:
                    return set_parse_error();
            }
        }
        return false;
    } else {
        auto [tag, size] = decode_tag(*text_begin);
        ++text_begin;
        switch (tag) {
            case bin_element_t::null:
                _result = nullptr;
                return true;
            case bin_element_t::undefined:
                _result = {};
                return true;
            case bin_element_t::boolean:
                _result = size != 0;
                return true;
            case bin_element_t::num_double:
                _stack.pop_back();
                _stack.push_back({state_type_t::bin_double});
                _stack.push_back({state_type_t::bin_number, 0,8});
                break;
            case bin_element_t::pos_number:
                _stack.pop_back();
                _stack.push_back({state_type_t::bin_number, 0,size+1});
                break;
            case bin_element_t::neg_number:
                _stack.pop_back();
                _stack.push_back({state_type_t::bin_neg_number, 0,size+1});
                break;
            case bin_element_t::array:
                _stack.pop_back();
                _stack.push_back({state_type_t::array});
                _stack.push_back({state_type_t::bin_number, 0,size+1});
                break;
            case bin_element_t::object:
                _stack.pop_back();
                _stack.push_back({state_type_t::object});
                _stack.push_back({state_type_t::bin_number, 0,size+1});
                break;
            case bin_element_t::num_string:
                _stack.pop_back();
                _stack.push_back({state_type_t::num_string});
                _stack.push_back({state_type_t::bin_number,0,size+1});
                break;
            case bin_element_t::string:
                _stack.pop_back();
                _stack.push_back({state_type_t::string});
                _stack.push_back({state_type_t::bin_number,0,size+1});
                break;
            case bin_element_t::bin_string:
                _stack.pop_back();
                _stack.push_back({state_type_t::bin_string});
                _stack.push_back({state_type_t::bin_number,0,size+1});
                break;
            case bin_element_t::sync:
                break;
            default:
                return set_parse_error();
        }
        return false;
    }

}



template<format_t format>
template<typename parser_t<format>::state_type_t type>
inline constexpr bool parser_t<format>::parse_string(state_t &st) {
    if constexpr(format == format_t::text) {
        if (eof) return set_parse_error();
        decode_json_string(_buffer.begin(), _buffer.end(), std::back_inserter(_decoded_buffer));
        std::string_view dstr(_decoded_buffer.begin(), _decoded_buffer.end());
        if (std::is_constant_evaluated()) {
            _result = alloc_str(dstr);
        } else {
            _result = value_t(dstr);
        }
        ++text_begin;
        _buffer.clear();
        _decoded_buffer.clear();
        return true;
    } else {
        if (st.number == 0) {
            st.number = _result.get();
            if (st.number == 0) {
                _result = std::string_view();
                return true;
            }
            _buffer.reserve(st.number);
         } else {
             while (text_begin != text_end && _buffer.size() < st.number) {
                 _buffer.push_back(*text_begin);
                 ++text_begin;
             }
             if (_buffer.size() == st.number) {
                 std::string_view dstr(_buffer.begin(), _buffer.end());
                 if (std::is_constant_evaluated()) {
                     if constexpr(type == state_type_t::num_string) {
                             _result = alloc_num_str(dstr);
                     } else if constexpr(type == state_type_t::bin_string) {
                             _result = alloc_bin_str(dstr);
                     } else {
                         static_assert(type == state_type_t::string);
                         _result = alloc_str(dstr);
                     }
                 } else {
                     if constexpr(type == state_type_t::num_string) {
                             _result = value_t(number_string_t(dstr));
                     } else if constexpr(type == state_type_t::bin_string) {
                             _result = value_t(binary_string_view_t(reinterpret_cast<const unsigned char *>(dstr.data()),dstr.size()));
                     } else {
                         static_assert(type == state_type_t::string);
                         _result = value_t(dstr);
                     }
                 }
                 _buffer.clear();
                 return true;
             }
         }
         return false;
    }
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_number(state_t &) {
    //todo - validate number
    number_string_t nstr({_buffer.begin(), _buffer.end()});
    if (!nstr.validate()) return set_parse_error();
    if (std::is_constant_evaluated()) {
        if (nstr.is_floating()) {
            _result = alloc_num_str(nstr);
        } else {
            _result = value_t(static_cast<long>(nstr));
        }
    } else {
        _result = value_t(nstr);
    }
    _buffer.clear();
    return true;
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_kw(state_t &action) {
    if constexpr(format == format_t::text) {
        if (eof) return set_parse_error();
        std::size_t p = 0;
        while (text_begin != text_end && p < action.str.size()) {
            if (*text_begin != action.str[p]) return set_parse_error();
            ++text_begin;
            ++p;
        }
        if (p == action.str.size()) {
            _result = std::move(action.key);
            return true;
        }
        action.str = action.str.substr(p);
        return false;
    } else {
        return set_parse_error();
    }
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_array_begin(state_t &) {
    if (eof) return set_parse_error();
    if (skip_ws()) {
        if (*text_begin == ']') {
            ++text_begin;
            _result = type_t::array;
            return true;
        }
        _stack.pop_back();
        _stack.push_back({state_type_t::array});
        _stack.push_back({state_type_t::detect});
    }
    return false;
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_array(state_t &action) {
    if constexpr(format == format_t::text) {
        if (eof) return set_parse_error();
        if (skip_ws()) {
            action.arr.push_back(std::move(_result));
            if (*text_begin == ']') {
                _result = value_t(action.arr);
                ++text_begin;
                return true;
            }
            if (*text_begin != ',') return set_parse_error();
            ++text_begin;
            _stack.push_back({state_type_t::detect});
        }
    } else {
        if (action.number == 0) {
            action.number = _result.get();
            if (action.number == 0) {
                _result = value_t(type_t::array);
                return true;
            }
            action.arr.reserve(action.number);
            _stack.push_back({state_type_t::detect});
        } else {
            action.arr.push_back(std::move(_result));
            if (action.arr.size() == action.number) {
                _result = value_t(action.arr);
                return true;
            }
            _stack.push_back({state_type_t::detect});
        }
    }
    return false;
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_object_begin(state_t &) {
    if (eof) return set_parse_error();
    if (skip_ws()) {
        if (*text_begin == '}') {
            ++text_begin;
            _result = type_t::object;
            return true;
        }
        _stack.pop_back();
        _stack.push_back({state_type_t::object});
        _stack.push_back({state_type_t::detect});
    }
    return false;

}

template<format_t format>
inline constexpr bool parser_t<format>::parse_object(state_t &action) {
    if constexpr(format == format_t::text) {
        if (eof) return set_parse_error();
        if (skip_ws()) {
            if ((action.arr.size() & 0x1) == 0) {
                if (_result.type() != type_t::string) return set_parse_error();
                if (*text_begin != ':') return set_parse_error();
                action.arr.push_back(std::move(_result));
            } else {
                action.arr.push_back(std::move(_result));
                if (*text_begin == '}') {
                    ++text_begin;
                    _result = make_object(action.arr);
                    return true;
                }
                if (*text_begin != ',') return set_parse_error();
            }
            ++text_begin;
            _stack.push_back({state_type_t::detect});
        }
    } else {
        if (action.number == 0) {
            action.number = _result.get();
            if (action.number == 0) {
                _result = value_t(type_t::object);
                return true;
            }
            action.number *= 2;
            action.arr.reserve(action.number);
            _stack.push_back({state_type_t::detect});
        } else {
            action.arr.push_back(std::move(_result));
            if (action.arr.size() == action.number) {
                _result = make_object(action.arr);
                return true;
            }
            _stack.push_back({state_type_t::detect});
        }
    }
    return false;
}


template<format_t format>
inline constexpr bool parser_t<format>::skip_ws() {
    if (eof) return true;
    while (text_begin != text_end) {
        if (!is_whitespace(*text_begin)) return true;
        ++text_begin;
    }
    return false;
}

template<format_t format>
constexpr value_t parser_t<format>::alloc_str(std::string_view s) {
    return value_t(shared_array_t<char>::create(s.size(), [&](auto from, auto){
            std::copy(s.begin(), s.end(), from);
        }),false);
}
template<format_t format>
constexpr value_t parser_t<format>::alloc_num_str(std::string_view s) {
    return value_t(shared_array_t<char>::create(s.size(), [&](auto from, auto){
            std::copy(s.begin(), s.end(), from);
        }),true);
}
template<format_t format>
constexpr value_t parser_t<format>::alloc_bin_str(std::string_view s) {
    return value_t(shared_array_t<unsigned char>::create(s.size(), [&](auto from, auto){
            std::copy(s.begin(), s.end(), from);
    }));
}


class parse_error_exception_t: public std::exception {
public:
    parse_error_exception_t(std::string_view at):_remain(at) {}
    std::string_view get_remain() const {return _remain;}
    const char *what() const noexcept override {
        return "JSON parse error";
    }
protected:
    std::string _remain;
};

constexpr value_t value_t::from_json(const std::string_view &text) {
    parser_t<format_t::text> pr={};
    pr.write(text);
    if (!pr.write("")) throw parse_error_exception_t("");
    if (pr.is_error()) throw parse_error_exception_t(pr.get_unused_text());
    return pr.get_parsed();
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_bin_number(state_t &action) {
    if constexpr(format == format_t::text) {
        return set_parse_error();
    } else {
        while (text_begin != text_end && action.byte_count) {
            action.number = (action.number << 8) | static_cast<unsigned char>(*text_begin);
            ++text_begin;
            --action.byte_count;
        }
        if (action.byte_count == 0) {
            _result = action.number;
            return true;
        }
        return false;
    }
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_bin_neg_number(state_t &) {
    std::int64_t r = _result.get();
    _result = -r;
    return true;
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_bin_double(state_t &) {
    std::uint64_t r = _result.get();
    _result = std::bit_cast<double>(r);
    return true;
}



template class parser_t<format_t::text>;
template class parser_t<format_t::binary>;

}
