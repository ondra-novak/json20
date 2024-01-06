#pragma once

#include <optional>
#include "number_string.h"
namespace json20 {

///undefined type - represents undefined value (monostate)
struct undefined_t {};

template<typename T>
concept null_or_undefined_t = std::is_same_v<T, undefined_t> || std::is_same_v<T, std::nullptr_t>;

template<typename From, typename To>
struct special_conversion_t {
    constexpr std::optional<To> operator()(const From &) const {return std::nullopt;}
};


template<null_or_undefined_t T>
struct special_conversion_t<T, double> {
    constexpr std::optional<double> operator()(T) const {
        return std::numeric_limits<double>::signaling_NaN();
    }
};

template<number_t T>
struct special_conversion_t<std::string_view, T> {
    constexpr std::optional<T> operator()(const std::string_view &str) const {
        number_string_t v(str);
        return T(v);
    }
};





}
