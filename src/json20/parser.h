#pragma once
#include <locale>
#include <string_view>
#include <vector>
#include <string>


namespace json20 {


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

    constexpr bool is_error() const {return parse_error;}

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
        skip_ws
    };

    struct state_t {
        state_type_t st;
        bool flag = false;
        std::string_view str = {};
        value_t key = {};
        std::vector<value_t> arr = {};
    };


    value_t _result;
    std::vector<char> _buffer;
    std::vector<char> _decoded_buffer;
    std::vector<state_t> _stack;
    const char *text_begin = 0;
    const char *text_end = 0;
    const char *text_parse_begin = 0;
    bool eof = false;
    bool parse_error = false;

    constexpr bool set_parse_error() {
        parse_error = true;
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
    constexpr bool parse_string(state_t &action);
    constexpr bool parse_number(state_t &action);
    constexpr bool parse_kw(state_t &action);
    constexpr bool parse_object(state_t &action);
    constexpr bool parse_object_begin(state_t &action);
    constexpr bool parse_array(state_t &action);
    constexpr bool parse_array_begin(state_t &action);
    constexpr bool skip_ws();

    static constexpr value_t alloc_str(std::string_view s, bool number);
    template<typename Iter, typename OutIter>
    static constexpr OutIter decode_json_string(Iter beg, Iter end, OutIter output);
    static constexpr int hex_to_int(char hex);
};

constexpr int parser_t::hex_to_int(char hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        return hex - 'a' + 10;
    }
    return 0;
}

template<typename Iter, typename OutIter>
constexpr OutIter parser_t::decode_json_string(Iter beg, Iter end, OutIter output) {
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



inline constexpr bool parser_t::write(const std::string_view &text) {
    eof = text.empty();
    text_begin = text.begin();
    text_end = text.end();
    if (parse_error || parse()) return true;
    while (text_begin != text_end) {
        if (parse_error || parse()) return true;
    }
    return parse_error;
}

inline constexpr bool parser_t::read_text_until(const std::string_view stop_chars, bool &flag) {

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

inline constexpr void parser_t::reset() {
    _stack.push_back({state_type_t::detect});
}

inline constexpr bool parser_t::parse_top() {
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
            return read_text_until(top.str, top.flag);
        case state_type_t::string:
            return parse_string(top);
        case state_type_t::object:
            return parse_object(top);
        case state_type_t::object_begin:
            return parse_object_begin(top);
        case state_type_t::skip_ws:
            return skip_ws();
    }
}


inline constexpr bool parser_t::parse() {
    while (!_stack.empty()) {
        if (!parse_top()) return false;
        _stack.pop_back();
    }
    return true;
}

inline constexpr bool parser_t::detect_value() {
    if (eof) return set_parse_error();
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

}




inline constexpr bool parser_t::parse_string(state_t &) {
    if (eof) return set_parse_error();
    decode_json_string(_buffer.begin(), _buffer.end(), std::back_inserter(_decoded_buffer));
    std::string_view dstr(_decoded_buffer.begin(), _decoded_buffer.end());
    if (std::is_constant_evaluated()) {
        _result = alloc_str(dstr, false);
    } else {
        _result = value_t(dstr);
    }
    ++text_begin;
    _buffer.clear();
    _decoded_buffer.clear();
    return true;
}

inline constexpr bool parser_t::parse_number(state_t &) {
    //todo - validate number
    number_string_t nstr({_buffer.begin(), _buffer.end()});
    if (!nstr.validate()) return set_parse_error();
    if (std::is_constant_evaluated()) {
        _result = alloc_str(nstr, true);
    } else {
        _result = value_t(nstr);
    }
    _buffer.clear();
    return true;
}

inline constexpr bool parser_t::parse_kw(state_t &action) {
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
}

inline constexpr bool parser_t::parse_array_begin(state_t &) {
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

inline constexpr bool parser_t::parse_array(state_t &action) {
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
    return false;
}

inline constexpr bool parser_t::parse_object_begin(state_t &) {
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

inline constexpr bool parser_t::parse_object(state_t &action) {
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
                shared_array_t<key_value_t> *kvarr = shared_array_t<key_value_t>::create(action.arr.size()/2,[&](auto *beg, auto *end){
                    auto iter = action.arr.begin();
                   while (beg != end) {
                       beg->key = std::move(*iter);
                       ++iter;
                       beg->value = std::move(*iter);
                       ++iter;
                       ++beg;

                   }
                });
                _result = value_t(kvarr);
                return true;
            }
            if (*text_begin != ',') return set_parse_error();
        }
        ++text_begin;
        _stack.push_back({state_type_t::detect});
    }
    return false;
}



inline constexpr bool parser_t::skip_ws() {
    if (eof) return true;
    while (text_begin != text_end) {
        if (!is_whitespace(*text_begin)) return true;
        ++text_begin;
    }
    return false;
}

constexpr value_t parser_t::alloc_str(std::string_view s, bool number) {
    auto ptr = shared_array_t<char>::create(s.size(), [&](auto from, auto){
        std::copy(s.begin(), s.end(), from);
    });
    return value_t(ptr, number);

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

constexpr value_t value_t::parse(const std::string_view &text) {
    parser_t pr;
    pr.write(text);
    if (!pr.write("")) throw parse_error_exception_t("");
    if (pr.is_error()) throw parse_error_exception_t(pr.get_unused_text());
    return pr.get_parsed();
}

}
