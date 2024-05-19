#pragma once
#include <locale>
#include <string_view>
#include <vector>
#include <string>
#include <any>
#include "serialize_common.h"


namespace json20 {

class parse_error_t: public std::exception {
public:

    enum error_t {
        unexpected_eof,
        unexpected_character,
        unexpected_separator,
        invalid_number,
        invalid_keyword,
        expected_key_as_string,
        internal_error_invalid_state,
        invalid_unicode,
        invalid_string_escape_sequence,
    };

    parse_error_t(error_t err, std::any iter): _err(err), _iter(iter) {}
    const char *what() const noexcept override {
        switch (_err) {
            default: return "unknown error";
            case unexpected_eof: return "unexpected eof";
            case unexpected_character: return "unexpected character";
            case unexpected_separator: return "unexpected separator";
            case invalid_number: return "invalid number";
            case invalid_keyword: return "invalid keyword";
            case expected_key_as_string: return "expected string as key";
            case internal_error_invalid_state: return "internal parser error";
            case invalid_unicode: return "invalid unicode";
            case invalid_string_escape_sequence: return "invalid string escape sequence";
        }
    }

    template<typename Iter>
    Iter get_position() const {
        return std::any_cast<Iter>(_iter);
    }

protected:
    error_t _err;
    std::any _iter;
};


class parser_t {
public:


    template<std::forward_iterator Iter>
    constexpr Iter parse(Iter iter, Iter end,  value &out);



protected:

    std::vector<value> _value_stack;
    std::vector<char> _str_buff;

    template<std::forward_iterator Iter>
    constexpr Iter eat_white(Iter iter, Iter end) {
        while (iter != end) {
            char c = *iter;
            if (c != ' ' && c != '\r' && c != '\n' && c != '\t') return iter;
            ++iter;
        }
        throw parse_error_t(parse_error_t::unexpected_eof, iter);
    }

    template<std::forward_iterator Iter>
    constexpr Iter check_kw(Iter iter, Iter end, const std::string_view &chk) {
        auto chkb = chk.begin();
        auto chke = chk.end();
        while (iter != end && chkb != chke) {
            if (*iter != *chkb) throw parse_error_t(parse_error_t::unexpected_character, iter);
            ++iter;
            ++chkb;
        }
        if (chkb != chke) throw parse_error_t(parse_error_t::unexpected_eof, iter);
        return iter;

    }

    template<std::forward_iterator Iter>
    constexpr Iter parse_object(Iter iter, Iter end, value &out);

    template<std::forward_iterator Iter>
    constexpr Iter parse_array(Iter iter, Iter end, value &out);

    template<std::forward_iterator Iter>
    constexpr Iter parse_string(Iter iter, Iter end, value &out);

    template<std::forward_iterator Iter>
    constexpr Iter parse_number(Iter iter, Iter end, value &out);

    static constexpr std::uint32_t build_bloom_mask(std::string_view chars) {
        std::uint32_t out = 0;
        for (auto c: chars) {
            out |= (1UL << (c & 0x1F));
        }
        return out;
    }
};



template<std::forward_iterator Iter>
constexpr Iter parser_t::parse(Iter iter, Iter end,  value &out) {
    iter = eat_white(iter, end);
    char c = *iter;
    if (c == '"') {
        ++iter;
        return parse_string(iter, end, out);
    } else if ((c >= '0' && c <= '9') || (c == '-')) {
        return parse_number(iter, end, out);
    } else {
        switch(c) {
            case '[':
                ++iter;
                return parse_array(iter, end, out);
            case '{':
                ++iter;
                return parse_object(iter, end, out);
            case 't':
                out = true;
                return check_kw(iter, end, "true");
            case 'f':
                out = false;
                return check_kw(iter, end, "false");
            case 'n':
                out = nullptr;
                return check_kw(iter, end, "null");
            default:
            {
                char z[1];
                z[(int)c] = 1;
                throw parse_error_t(parse_error_t::unexpected_character, iter);
            }
        }
    }
}

template<std::forward_iterator Iter>
constexpr Iter parser_t::parse_string(Iter iter, Iter end, value &out) {
    if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
    if (*iter == '"') {
        out = std::string_view();
        return iter;
    }
    int first_codepoint = 0;
    Iter prev = iter;
    Iter strend = iter;
    ++strend;
    do {
        strend = std::find(strend, end, '"');
        std::advance(prev, std::distance(prev, strend)-1);
        if (*prev != '\\') break;
        prev = strend;
        ++strend;
    } while (true);
    auto needbuff = std::distance(iter, strend);
    _str_buff.resize(needbuff,0);
    auto wr = _str_buff.begin();
    while (iter != strend) {
        if (*iter != '\\') {
            iter = std::find_if(iter, strend, [&](char c){
                if (c == '\\') return true;
                *wr++ = c;
                return false;
            });
        } else {
            ++iter;
            if (iter == strend) {
                throw parse_error_t(parse_error_t::invalid_string_escape_sequence, iter);
            }
            switch (*iter) {
                default: throw parse_error_t(parse_error_t::invalid_string_escape_sequence, iter);
                case '\\':
                case '"': *wr = *iter; break;
                case 'b':*wr = '\b';break;
                case 'f':*wr = '\f';break;
                case 'n':*wr = '\n';break;
                case 'r':*wr = '\r';break;
                case 't':*wr = '\t';break;
                case 'u': {
                    int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        ++iter;
                        if (iter == strend) {
                            throw parse_error_t(parse_error_t::invalid_unicode, iter);
                        }
                        unsigned char c = *iter;
                        if (c >= 'A') {
                            c = (c | 0x20) - 87;
                            if (c > 0xF) throw parse_error_t(parse_error_t::invalid_unicode, iter);

                        } else {
                            c -= '0';
                            if (c > 9) throw parse_error_t(parse_error_t::invalid_unicode, iter);
                        }

                        codepoint=(codepoint << 4) | c;
                    }
                    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        if (first_codepoint) {
                            if (first_codepoint > codepoint) std::swap(first_codepoint, codepoint);
                            codepoint = 0x10000 + ((first_codepoint - 0xD800) << 10) + (codepoint - 0xDC00);
                        } else {
                            first_codepoint = codepoint;
                            ++iter;
                            continue;
                        }
                    }
                    if (codepoint <= 0x7F) {
                        *wr = static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        *wr = static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                        ++wr;
                        *wr = static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0xFFFF) {
                        *wr = static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                        ++wr;
                        *wr = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        ++wr;
                        *wr = static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0x10FFFF) {
                        *wr = static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
                        ++wr;
                        *wr = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        ++wr;
                        *wr = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        ++wr;
                        *wr = static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    first_codepoint = 0;
                }
                break;
            }
            ++wr;
            ++iter;
        }
    }
    std::string_view s(_str_buff.begin(), wr);
    if (std::is_constant_evaluated()) {
        out = value(shared_array_t<char>::create(s.size(), [&](auto from, auto){
                std::copy(s.begin(), s.end(), from);
        }),false);

    } else {
        out = s;
    }
    ++iter;
    return iter;
}


template<std::forward_iterator Iter>
constexpr Iter parser_t::parse_number(Iter iter, Iter end, value &out) {
    constexpr auto srch_mask = build_bloom_mask("0123456789-+.Ee");
    constexpr auto is_digit = [](char c) {
        return c>='0' && c <= '9';
    };
    auto vend = std::find_if(iter, end, [](char c){
        return !(srch_mask & (1UL << (c & 0x1F)));
    });
    auto size = std::distance(iter, vend);
    _str_buff.resize(size+1,0);
    *std::copy(iter, vend, _str_buff.begin()) = '\0';

    auto chk = _str_buff.begin();
    if (*chk == '-') ++chk;
    if (*chk == '0') {
        ++chk;
    } else {
        if (!is_digit(*chk)) throw parse_error_t(parse_error_t::invalid_number, iter);
        ++chk;
        while (is_digit(*chk)) ++chk;
    }
    if (*chk == '.') {
        ++chk;
        if (!is_digit(*chk)) throw parse_error_t(parse_error_t::invalid_number, iter);
        ++chk;
        while (is_digit(*chk)) ++chk;
    }
    if (*chk == 'e' || *chk == 'E') {
        ++chk;
        if (*chk == '-' || *chk == '+') ++chk;
        if (!is_digit(*chk)) throw parse_error_t(parse_error_t::invalid_number, iter);
        ++chk;
        while (is_digit(*chk)) ++chk;
    }
    auto sz = std::distance(_str_buff.begin(), chk);
    auto nstr =  number_string_t(std::string_view(_str_buff.data(), sz));
    if (std::is_constant_evaluated()) {
        if (nstr.is_floating()) {
            out = value(shared_array_t<char>::create(nstr.size(), [&](auto from, auto){
                           std::copy(nstr.begin(), nstr.end(), from);
            }),false);
        } else {
            out = value(static_cast<long>(nstr));
        }
    } else {
        out = value(nstr);
    }
    std::advance(iter, sz);
    return iter;

}

template<std::forward_iterator Iter>
constexpr Iter parser_t::parse_array(Iter iter, Iter end, value &out) {
    iter = eat_white(iter, end);
    if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
    if (*iter == ']') {
        out = value(type::array);
        ++iter;
        return iter;
    }
    auto stpos = _value_stack.size();
    do {
        iter = parse(iter, end, out);
        _value_stack.push_back(std::move(out));
        iter = eat_white(iter, end);
        if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
        if (*iter == ']') break;
        if (*iter != ',') throw parse_error_t(parse_error_t::unexpected_separator, iter);
        ++iter;
        iter = eat_white(iter, end);
        if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
    } while (true);
    auto arr = shared_array_t<value>::create(_value_stack.size() - stpos,[&](auto from, auto){
        for (std::size_t i = stpos, cnt = _value_stack.size(); i < cnt; ++i) {
            *from = std::move(_value_stack[i]);
            ++from;
        }
    });
    out = value(arr);
    _value_stack.resize(stpos);
    ++iter;
    return iter;
}

template<std::forward_iterator Iter>
constexpr Iter parser_t::parse_object(Iter iter, Iter end, value &out) {
    iter = eat_white(iter, end);
    if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
    if (*iter == '}') {
        out = value(type::object);
        ++iter;
        return iter;
    }
    auto stpos = _value_stack.size();
    do {
        if (*iter != '"') throw parse_error_t(parse_error_t::expected_key_as_string, iter);
        ++iter;
        iter = parse_string(iter, end, out);
        _value_stack.push_back(std::move(out));
        iter = eat_white(iter, end);
        if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
        if (*iter != ':') throw parse_error_t(parse_error_t::unexpected_separator, iter);
        ++iter;
        iter = eat_white(iter, end);
        if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
        iter = parse(iter, end, out);
        _value_stack.push_back(std::move(out));
        iter = eat_white(iter, end);
        if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
        if (*iter == '}') break;
        if (*iter != ',') throw parse_error_t(parse_error_t::unexpected_separator, iter);
        ++iter;
        iter = eat_white(iter, end);
        if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
    }
    while (true);
    auto cnt = (_value_stack.size() - stpos)/2;
    auto arr = shared_array_t<key_value>::create(cnt,[&](auto from, auto){
        for (std::size_t i = 0; i < cnt; ++i) {
            from->key = std::move(_value_stack[stpos+i*2]);
            from->value = std::move(_value_stack[stpos+i*2+1]);
            ++from;
        }
    });
    out = value(arr);
    _value_stack.resize(stpos);
    ++iter;
    return iter;
}

constexpr value value::from_json(const std::string_view &text) {
    parser_t pr;
    value out;
    pr.parse(text.begin(), text.end(), out);
    return out;
}



#if 0

template<format_t format  = format_t::text>
class parser_t {
public:

    constexpr parser_t() {reset();}

    constexpr bool write(const std::string_view &text);

    constexpr const value &get_parsed() const {
        return _result;
    }

    constexpr std::string_view get_unused_text() const {
        return std::string_view(text_begin, text_end);
    }

    constexpr void reset();

    constexpr bool is_error() const {return _parse_error || !_is_done;}

    constexpr bool is_done() const {return _is_done;}

protected:

    enum class state_type {
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
        state_type st;
        bool flag = false;
        std::string_view str = {};
        value key = {};
        std::vector<value> arr = {};
    };

    struct binary_state_t {
        state_type st;
        std::uint64_t number = 0;
        unsigned int byte_count = 0;
        value key = {};
        std::vector<value> arr = {};
    };

    using state_t = std::conditional_t<format == format_t::text, text_state_t, binary_state_t>;

    value _result;
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
    template<state_type type>
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

    static constexpr value alloc_str(std::string_view s);
    static constexpr value alloc_num_str(std::string_view s);
    static constexpr value alloc_bin_str(std::string_view s);
    template<typename Iter, typename OutIter>
    static constexpr OutIter decode_json_string(Iter beg, Iter end, OutIter output);
    static constexpr int hex_to_int(char hex);


    constexpr value make_object(std::vector<value> &keypairs) {
        shared_array_t<key_value> *kvarr = shared_array_t<key_value>::create(keypairs.size()/2,[&](auto *beg, auto *end){
            auto iter = keypairs.begin();
           while (beg != end) {
               beg->key = std::move(*iter);
               ++iter;
               beg->value = std::move(*iter);
               ++iter;
               ++beg;

           }
        });
        return value(kvarr);
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
    _stack.push_back({state_type::detect});
    _is_done = false;
}

template<format_t format>
inline constexpr bool parser_t<format>::parse_top() {
    auto &top = _stack.back();
    switch (top.st) {
        case state_type::detect:
            return detect_value();
        case state_type::array_begin:
            return parse_array_begin(top);
        case state_type::array:
            return parse_array(top);
        case state_type::number:
            return parse_number(top);
        case state_type::read_kw:
            return parse_kw(top);
        case state_type::read_until:
            if constexpr(format == format_t::text) {
                return read_text_until(top.str, top.flag);
            }
            return true;
        case state_type::string:
            return parse_string<state_type::string>(top);
        case state_type::object:
            return parse_object(top);
        case state_type::object_begin:
            return parse_object_begin(top);
        case state_type::skip_ws:
            return skip_ws();
        case state_type::bin_number:
            return parse_bin_number(top);
        case state_type::bin_neg_number:
            return parse_bin_neg_number(top);
        case state_type::bin_double:
            return parse_bin_double(top);
        case state_type::bin_string:
            return parse_string<state_type::bin_string>(top);
        case state_type::num_string:
            return parse_string<state_type::num_string>(top);
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
                case '[': _stack.push_back({state_type::array_begin});
                          break;
                case '{': _stack.push_back({state_type::object_begin});
                          break;
                case '"': _stack.push_back({state_type::string});
                          _stack.push_back({state_type::read_until, false, "\""});
                          break;
                case value::str_true[0]: _stack.push_back({state_type::read_kw, false, value::str_true, true});
                          --text_begin;
                          break;
                case value::str_false[0]: _stack.push_back({state_type::read_kw, false, value::str_false, false});
                          --text_begin;
                          break;
                case value::str_null[0]: _stack.push_back({state_type::read_kw, false, value::str_null, nullptr});
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
                case '+': _stack.push_back({state_type::number});
                          _stack.push_back({state_type::read_until, false, ",}] \t\r\n"});
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
                _stack.push_back({state_type::bin_double});
                _stack.push_back({state_type::bin_number, 0,8});
                break;
            case bin_element_t::pos_number:
                _stack.pop_back();
                _stack.push_back({state_type::bin_number, 0,size+1});
                break;
            case bin_element_t::neg_number:
                _stack.pop_back();
                _stack.push_back({state_type::bin_neg_number, 0,size+1});
                break;
            case bin_element_t::array:
                _stack.pop_back();
                _stack.push_back({state_type::array});
                _stack.push_back({state_type::bin_number, 0,size+1});
                break;
            case bin_element_t::object:
                _stack.pop_back();
                _stack.push_back({state_type::object});
                _stack.push_back({state_type::bin_number, 0,size+1});
                break;
            case bin_element_t::num_string:
                _stack.pop_back();
                _stack.push_back({state_type::num_string});
                _stack.push_back({state_type::bin_number,0,size+1});
                break;
            case bin_element_t::string:
                _stack.pop_back();
                _stack.push_back({state_type::string});
                _stack.push_back({state_type::bin_number,0,size+1});
                break;
            case bin_element_t::bin_string:
                _stack.pop_back();
                _stack.push_back({state_type::bin_string});
                _stack.push_back({state_type::bin_number,0,size+1});
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
template<typename parser_t<format>::state_type type>
inline constexpr bool parser_t<format>::parse_string(state_t &st) {
    if constexpr(format == format_t::text) {
        if (eof) return set_parse_error();
        decode_json_string(_buffer.begin(), _buffer.end(), std::back_inserter(_decoded_buffer));
        std::string_view dstr(_decoded_buffer.begin(), _decoded_buffer.end());
        if (std::is_constant_evaluated()) {
            _result = alloc_str(dstr);
        } else {
            _result = value(dstr);
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
                     if constexpr(type == state_type::num_string) {
                             _result = alloc_num_str(dstr);
                     } else if constexpr(type == state_type::bin_string) {
                             _result = alloc_bin_str(dstr);
                     } else {
                         static_assert(type == state_type::string);
                         _result = alloc_str(dstr);
                     }
                 } else {
                     if constexpr(type == state_type::num_string) {
                             _result = value(number_string_t(dstr));
                     } else if constexpr(type == state_type::bin_string) {
                             _result = value(binary_string_view_t(reinterpret_cast<const unsigned char *>(dstr.data()),dstr.size()));
                     } else {
                         static_assert(type == state_type::string);
                         _result = value(dstr);
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
            _result = value(static_cast<long>(nstr));
        }
    } else {
        _result = value(nstr);
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
            _result = type::array;
            return true;
        }
        _stack.pop_back();
        _stack.push_back({state_type::array});
        _stack.push_back({state_type::detect});
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
                _result = value(action.arr);
                ++text_begin;
                return true;
            }
            if (*text_begin != ',') return set_parse_error();
            ++text_begin;
            _stack.push_back({state_type::detect});
        }
    } else {
        if (action.number == 0) {
            action.number = _result.get();
            if (action.number == 0) {
                _result = value(type::array);
                return true;
            }
            action.arr.reserve(action.number);
            _stack.push_back({state_type::detect});
        } else {
            action.arr.push_back(std::move(_result));
            if (action.arr.size() == action.number) {
                _result = value(action.arr);
                return true;
            }
            _stack.push_back({state_type::detect});
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
            _result = type::object;
            return true;
        }
        _stack.pop_back();
        _stack.push_back({state_type::object});
        _stack.push_back({state_type::detect});
    }
    return false;

}

template<format_t format>
inline constexpr bool parser_t<format>::parse_object(state_t &action) {
    if constexpr(format == format_t::text) {
        if (eof) return set_parse_error();
        if (skip_ws()) {
            if ((action.arr.size() & 0x1) == 0) {
                if (_result.type() != type::string) return set_parse_error();
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
            _stack.push_back({state_type::detect});
        }
    } else {
        if (action.number == 0) {
            action.number = _result.get();
            if (action.number == 0) {
                _result = value(type::object);
                return true;
            }
            action.number *= 2;
            action.arr.reserve(action.number);
            _stack.push_back({state_type::detect});
        } else {
            action.arr.push_back(std::move(_result));
            if (action.arr.size() == action.number) {
                _result = make_object(action.arr);
                return true;
            }
            _stack.push_back({state_type::detect});
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
constexpr value parser_t<format>::alloc_str(std::string_view s) {
    return value(shared_array_t<char>::create(s.size(), [&](auto from, auto){
            std::copy(s.begin(), s.end(), from);
        }),false);
}
template<format_t format>
constexpr value parser_t<format>::alloc_num_str(std::string_view s) {
    return value(shared_array_t<char>::create(s.size(), [&](auto from, auto){
            std::copy(s.begin(), s.end(), from);
        }),true);
}
template<format_t format>
constexpr value parser_t<format>::alloc_bin_str(std::string_view s) {
    return value(shared_array_t<unsigned char>::create(s.size(), [&](auto from, auto){
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

#endif
}
