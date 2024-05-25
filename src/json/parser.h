#pragma once
#include "serialize_common.h"



namespace JSON20_NAMESPACE_NAME {

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


    template<std::forward_iterator Iter>
    constexpr Iter parse_binary(Iter iter, Iter end, value &out);


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
                throw parse_error_t(parse_error_t::unexpected_character, iter);
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
    auto nstr =  number_string(std::string_view(_str_buff.data(), sz));
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
            from->key = key_t::from_value(std::move(_value_stack[stpos+i*2]));
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

template<std::forward_iterator Iter>
constexpr Iter parser_t::parse_binary(Iter iter, Iter end, value &out) {
    if (iter == end) throw parse_error_t(parse_error_t::unexpected_eof, iter);
    char tagbyte = *iter;
    ++iter;
    std::uint64_t len;
    bin_element_t tag = static_cast<bin_element_t>(tagbyte & 0xF0);
    switch (tag) {
        case bin_element_t::sync:
            switch (static_cast<bin_element_t>(tagbyte)) {
                case bin_element_t::sync:
                    return parse_binary(iter, end, out);
                case bin_element_t::undefined:
                    out = undefined;
                    break;
                case bin_element_t::null:
                    out = nullptr;
                    break;
                case bin_element_t::bool_true:
                    out = true;
                    break;
                case bin_element_t::bool_false:
                    out = false;
                    break;
                case bin_element_t::num_double: {
                    std::uint64_t v = 0;
                    for (int i = 0; i < 8; ++i) {
                        v |= static_cast<std::uint64_t>(
                                static_cast<unsigned char>(*iter)) << (8*i);
                        ++iter;
                    }
                    out = std::bit_cast<double>(v);
                    break;
                }
                default: throw parse_error_t(parse_error_t::unexpected_character, iter);
            }
            return iter;
        case bin_element_t::string:
        case bin_element_t::num_string:
            iter = parse_len(iter, end, len);
            out = value(shared_array_t<char> ::create(len,[&](auto wr, auto wrend){
                while (iter != end && wr != wrend) {
                    *iter++ = *wr++;
                }
            }),tag == bin_element_t::num_string);
            return iter;
        case bin_element_t::bin_string:
            iter = parse_len(iter, end, len);
            out = value(shared_array_t<unsigned char> ::create(len,[&](auto wr, auto wrend){
                while (iter != end && wr != wrend) {
                    *iter++ = *wr++;
                }
            }));
            return iter;
        case bin_element_t::pos_number:
            iter = parse_len(iter, end, len);
            out = value(len);
            return iter;
        case bin_element_t::neg_number:
            iter = parse_len(iter, end, len);
            out = value(-static_cast<std::int64_t>(len));
            return iter;
        case bin_element_t::array:
            iter = parse_len(iter, end, len);
            out = value(shared_array_t<value> ::create(len,[&](auto wr, auto wrend){
                while (iter != end && wr != wrend) {
                    iter = parse_binary(iter, end, *wr);
                    ++wr;
                }
            }));
            return iter;
        case bin_element_t::object:
            iter = parse_len(iter, end, len);
            out = value(shared_array_t<key_value> ::create(len,[&](auto wr, auto wrend){
                while (iter != end && wr != wrend) {
                    iter = parse_binary(iter, end, wr->key);
                    iter = parse_binary(iter, end, wr->value);
                    ++wr;
                }
            }));
            return iter;
        default: throw parse_error_t(parse_error_t::unexpected_character, iter);
    }
}

}
