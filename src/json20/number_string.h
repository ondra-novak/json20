#pragma once

#include <cctype>
#include <limits>
#include <string_view>

namespace json20 {

template<typename T> concept signed_integer_number_t =
          std::is_same_v<T, short int> || std::is_same_v<T, int> ||
          std::is_same_v<T, long> || std::is_same_v<T, long long>;

template<typename T> concept unsigned_integer_number_t =
        std::is_same_v<T, unsigned short int> || std::is_same_v<T, unsigned int> ||
        std::is_same_v<T, unsigned long> || std::is_same_v<T, unsigned long long>;
template<typename T> concept number_t =
        signed_integer_number_t<T> || unsigned_integer_number_t<T> || std::is_floating_point_v<T>;


template<typename T>
concept optional_signed_integer_number_t = signed_integer_number_t<T> || std::constructible_from<T, decltype(std::nullopt)>;
template<typename T>
concept optional_unsigned_integer_number_t = unsigned_integer_number_t<T> || std::constructible_from<T, decltype(std::nullopt)>;

class pows10_table_t {
public:

    static constexpr int minexp = std::numeric_limits<double>::min_exponent10;
    static constexpr int maxexp = std::numeric_limits<double>::max_exponent10;
    static constexpr int table_size = maxexp - minexp + 1;

    constexpr pows10_table_t() {
        table[-minexp] = 1.0;
        double v = 1.0;
        for (int i = 1; i <= maxexp; ++i) {
            v = v * 10;
            table[i-minexp] = v;
        }
        v = 1.0;
        for (int i = -1; i >= minexp; --i) {
            v = v /10;
            table[i - minexp] = v;
        }
    }

    constexpr double pow10(int exponent) const{
        if (exponent<minexp) return -std::numeric_limits<double>::infinity();
        if (exponent>maxexp) return +std::numeric_limits<double>::infinity();
        return table[exponent-minexp];
    }

    constexpr int log10(double number) const {
        if (number > 0) {
            auto low = minexp;
            auto high = maxexp+1;
            while (low < high) {
                auto mid = (low+ high - 2*minexp)/2 + minexp;
                auto v = pow10(mid);
                auto adj = number/v;
                if (adj < 1.0) high = mid;
                else if (adj >= 10.0) low = mid+1;
                else return mid;

            }
            return low;
        } else {
            return 0;
        }
    }

protected:
    double table[table_size];

};

inline constexpr bool is_nan(double b) {
    return !(b>=-std::numeric_limits<double>::infinity() && b<=std::numeric_limits<double>::infinity());
}
inline constexpr bool is_neg_infinity(double b) {
    return b==-std::numeric_limits<double>::infinity();
}
inline constexpr bool is_pos_infinity(double b) {
    return b==std::numeric_limits<double>::infinity();
}




///allows to store a number as a string
/**
 * This increases speed of parsing and reduces possibility of loosing accuracy of
 * numbers when source is parsed and then serialized back.
 *
 * You can also use class number_string to put numbers to the JSON in optimaly
 * crafted accuracy
 *
 */
class number_string: public std::string_view {
public:
    constexpr number_string(const std::string_view &x):std::string_view(x) {}

    constexpr bool validate() const;
    constexpr bool is_floating() const {
        return find_first_of(".eE") != npos;
    }

    static constexpr std::string_view plus_infinity = "+\xe2\x88\x9e";  // "+∞"
    static constexpr std::string_view minus_infinity = "-\xe2\x88\x9e"; // "-∞"
    static constexpr pows10_table_t pows10_table = {};

    constexpr double parse() const;
    template<optional_signed_integer_number_t T>
    constexpr T parse_int() const;
    template<optional_unsigned_integer_number_t T>
    constexpr T parse_uint() const;
    constexpr operator double() const;
    constexpr operator int() const;
    constexpr operator long() const;
    constexpr operator long long() const;
    constexpr operator unsigned int() const;
    constexpr operator unsigned long() const;
    constexpr operator unsigned long long() const;


    template<signed_integer_number_t T>
    static constexpr std::optional<T> parse_int(std::string_view text);
    template<unsigned_integer_number_t T>
    static constexpr std::optional<T> parse_uint(std::string_view text);
    static constexpr double pow10(int e)
    {
        return pows10_table.pow10(e);
    }
    //calculate exponent, must be positive number (not work with negative)
    static constexpr int get_exponent(double number) {
        return pows10_table.log10(number);
    }
    static constexpr bool isdigit(char c) {
        return c >= '0' && c <= '9';
    }
};



inline constexpr double number_string::parse() const {
    std::string_view tmp = *this;
    if (tmp == plus_infinity) return std::numeric_limits<double>::infinity();
    if (tmp == minus_infinity) return -std::numeric_limits<double>::infinity();
    double mult = 1.0;
    if (tmp.front() == '+') {
        tmp = tmp.substr(1);
    } else if (tmp.front() == '-') {
        mult = -1.0;
        tmp = tmp.substr(1);
    }
    double val = 0.0;
    auto iter = tmp.begin();
    auto end = tmp.end();
    while (iter != end) {
        char c = *iter;
        if (c == '.' || c == 'E' || c == 'e') break;
        if (!isdigit(c)) return std::numeric_limits<double>::signaling_NaN();
        val = val * 10 + (c - '0');
        ++iter;
    }
    if (iter != end && *iter == '.') {
        ++iter;
        int dst = -1;
        while (iter != end) {
            char c = *iter;
            if (c == 'e' || c == 'E') break;
            if (!isdigit(c)) return std::numeric_limits<double>::signaling_NaN();
            val = val + pow10(dst) * (c - '0');
            ++iter;
            --dst;
        }
    }
    if (iter != end && (*iter == 'e' || *iter == 'E')) {
        ++iter;
        auto exponent = parse_int<int>(std::string_view(iter, end));
        if (!exponent.has_value()) return std::numeric_limits<double>::signaling_NaN();
        val = val * pow10(*exponent);
    }
    return mult * val;

}

template<signed_integer_number_t T>
inline constexpr std::optional<T> number_string::parse_int(std::string_view text) {
    if (text == plus_infinity) return std::numeric_limits<T>::max();
    if (text == minus_infinity) return std::numeric_limits<T>::min();
    bool neg = false;
    if (text.front() == '+') {
        text = text.substr(1);
    } else if (text.front() == '-') {
        neg = true;
        text = text.substr(1);
    }
    T val = 0;
    for (char c: text) {
        if (!isdigit(c)) return {};
        val = (val * 10) + (c - '0');
    }
    if (neg) val = -val;
    return val;

}

template<unsigned_integer_number_t T>
inline constexpr std::optional<T> number_string::parse_uint(std::string_view text) {
    if (text == plus_infinity) return std::numeric_limits<T>::max();
    if (text == minus_infinity) return std::numeric_limits<T>::min();
    if (text.front() == '+') {
        text = text.substr(1);
    } else if (text.front() == '-') {
        return {};
    }
    T val = 0;
    for (char c: text) {
        if (!isdigit(c)) return {};
        val = (val * 10) + (c - '0');
    }
    return val;
}

template<optional_signed_integer_number_t T>
inline constexpr T number_string::parse_int() const {
    if (empty()) return T();
    if (is_floating()) return static_cast<T>(parse());
    auto r = parse_int<T>(*this);
    return r.has_value()?*r:T();
}
template<optional_unsigned_integer_number_t T>
inline constexpr T number_string::parse_uint() const {
    if (empty()) return T();
    if (is_floating()) return static_cast<T>(parse());
    auto r = parse_uint<T>(*this);
    return r.has_value()?*r:T();
}

constexpr number_string::operator double() const {return parse();}
constexpr number_string::operator int() const {return static_cast<int>(parse_int<int>());}
constexpr number_string::operator long() const {return static_cast<long>(parse_int<long>());}
constexpr number_string::operator long long() const {return static_cast<long long>(parse_int<long long>());}
constexpr number_string::operator unsigned int() const {return static_cast<unsigned int>(parse_uint<unsigned int>());}
constexpr number_string::operator unsigned long() const {return static_cast<unsigned long>(parse_uint<unsigned long>());}
constexpr number_string::operator unsigned long long() const {return static_cast<unsigned long long>(parse_uint<unsigned long long>());}

inline constexpr bool number_string::validate() const {
    return !is_nan(parse());
}




}
