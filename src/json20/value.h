#pragma once

#include "spec_conv.h"
#include "shared_array.h"
#include <span>
#include <tuple>
#include <algorithm>
#include <memory>

namespace json20 {


///holds key and value
struct key_value_t;

///standard JSON value type
enum class type_t {
    ///value is not defined (nether value nor null)
    undefined,
    ///value is null
    null,
    ///value is boolean
    boolean,
    ///value is number
    number,
    ///value is string
    string,
    ///value is object (associated array)
    object,
    ///value is array (zero base index)
    array
};


class list_item_t;

class value_t;

using array_t = std::span<const value_t>;
using object_t = std::span<const key_value_t>;

/*
 * string < 16 letters
 *
 * +-+-+-+-+-+-+-+-+---------------+---------------+---------------+---    -----+
 * | | | | |i| | | |               |               |               |            |
 * |       |s| | |k|                                                            |
 * | size  |n|0|0|e|                     text 15 bytes                  ...     |
 * |       |u| | |y|                                                            |
 * | | | | |m| | | |               |               |               |            |
 * +-+-+-+-+-+-+-+-+---------------+---------------+---------------+---    -----+
 *
 * other types
 *
 * +-+-+-+-+-+-+-+-+---------------+---------------+---------------+
 * | | | | | | | | |               |               |               |
 * |         | | |k|               |               |               |
 * | type    |1|0|e|  align        |  align        |  align        |
 * |         | | |y|               |               |               |
 * | | | | | | | | |               |               |               |
 * +-+-+-+-+-+-+-+-+---------------+---------------+---------------+
 * |                                                               |
 * |   4 bytes           size (for some types)                     |
 * |                                                               |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |   8 bytes - payload                                           |
 * |                                                               |
 * |   number                                                      |
 * |   pointer                                                     |
 * |   bool (1 byte)                                               |
 * |                                                               |
 * |                                                               |
 * +---------------------------------------------------------------+
 *  isnum - when 1, the text represents a number (in text form)
 *  key - when 1, this value has associated key which follows
 *
 */


class value_t {
public:

    static constexpr std::string_view str_null = "null";
    static constexpr std::string_view str_true = "true";
    static constexpr std::string_view str_false = "false";
    static constexpr std::string_view str_undefined = "undefined";

    constexpr value_t():_data{undef, 0, 0, {}} {

    }
    constexpr ~value_t();

    constexpr value_t(const value_t &other) {
        if (other._data.type < undef) {
            std::construct_at(&_str, other._str);
            _str.has_key = 0;
        } else {
            std::construct_at(&_data, other._data);
            _data.has_key = 0;
            switch (_data.type) {
                case array: _data.array->add_ref();break;
                case object: _data.object->add_ref();break;
                case number_string:
                case string: _data.shared_str->add_ref();break;
                default:break;
            }
        }
    }

    constexpr value_t( value_t &&other) {
        if (other._data.type < undef) {
            std::construct_at(&_str, other._str);
            _str.has_key = 0;
        } else {
            std::construct_at(&_data, other._data);
            other._data.type = undef;
            _data.has_key = 0;
        }

    }

    constexpr value_t &operator=(const value_t &other) {
        if (this != &other) {
            std::destroy_at(this);
            std::construct_at(this, other);
        }
        return *this;
    }

    constexpr value_t &operator=( value_t &&other) {
        if ( this != &other) {
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
        }
        return *this;
    }


    constexpr value_t(std::nullptr_t):_data{null,0,0,{}} {}
    constexpr value_t(const char *text):value_t(std::string_view(text)) {}
    constexpr value_t(bool b):_data{boolean,0,0, {.b = b}} {}
    constexpr value_t(int val):_data{vint,0,0,{.vint = val}} {}
    constexpr value_t(unsigned int val):_data{vuint,0,0,{.vuint = val}} {}
    constexpr value_t(long val):_data{vlong,0,0,{.vlong = val}} {}
    constexpr value_t(unsigned long val):_data{vulong,0,0,{.vulong = val}} {}
    constexpr value_t(long long val):_data{vllong,0,0,{.vllong = val}} {}
    constexpr value_t(unsigned long long val):_data{vullong,0,0,{.vullong = val}} {}
    constexpr value_t(double val):_data{d,0,0,{.d = val}} {}
    constexpr value_t(const std::initializer_list<list_item_t> &lst);
    constexpr value_t(const array_t &arr);
    constexpr value_t(const object_t &obj);
    explicit constexpr value_t(shared_array_t<value_t> *arr):_data{array, 0, 0, {.array = arr}} {}
    explicit constexpr value_t(shared_array_t<key_value_t> *obj):_data{object, 0, 0, {.object = obj}} {
        sort_object(_data.object);
    }
    explicit constexpr value_t(const std::initializer_list<list_item_t> *lst):_data{list, 0,0,{.list = lst}} {}
    constexpr value_t(shared_array_t<char> *str, bool number):_data{number?number_string:string, 0, 0, {.shared_str = str}} {}
    constexpr value_t(undefined_t, std::string_view sview):_data{string_view, 0, static_cast<std::uint32_t>(sview.size()), {.str_view = sview.data()}} {}

    constexpr value_t(std::string_view text) {
        if (std::is_constant_evaluated()) {
            std::construct_at(&_data, Data{string_view, 0,static_cast<std::uint32_t>(text.size()), {text.data()}});
        } else {
            if (text.size() < 16) {
                std::construct_at(&_str);
                _str.size = static_cast<unsigned char>(text.size());
                std::copy(text.begin(), text.end(), _str.str);
            } else {
                _data = {string,0, 0,{.shared_str = shared_array_t<char>::create(text.size(),[&](char *beg, char *){
                    std::copy(text.begin(), text.end(), beg);
                })}};
            }
        }
    }

    constexpr value_t(const number_string_t &text) {
        if (std::is_constant_evaluated()) {
            std::construct_at(&_data, Data{number_string_view, 0,static_cast<std::uint32_t>(text.size()), {text.data()}});
        } else {
            if (text.size() < 16) {
                std::construct_at(&_str);
                _str.size = static_cast<unsigned char>(text.size());
                _str.isnum = 1;
                std::copy(text.begin(), text.end(), _str.str);
            } else {
                _data = {number_string,0, 0,{.shared_str = shared_array_t<char>::create(text.size(),[&](char *beg, char *){
                    std::copy(text.begin(), text.end(), beg);
                })}};
            }
        }

    }


    constexpr value_t(const value_t *elements, std::size_t sz, bool is_object)
        :_data({is_object?c_object:c_array,0,static_cast<std::uint32_t>(sz),{.c_array = elements}}) {}


    constexpr value_t(type_t type) {
        switch (type) {
            case type_t::undefined: std::construct_at(&_data, Data{undef,0,0,{}}); break;
            case type_t::null: std::construct_at(&_data, Data{null,0,0,{}}); break;
            case type_t::boolean: std::construct_at(&_data, Data{boolean,0,0,{}}); break;
            case type_t::string: std::construct_at(&_str, LocalStr()); break;
            case type_t::number: std::construct_at(&_data, Data{vint,0,0,{}}); break;
            case type_t::array: std::construct_at(&_data, Data{c_array,0,0,{.c_array = nullptr}}); break;
            case type_t::object: std::construct_at(&_data, Data{c_object,0,0,{.c_array = nullptr}}); break;
        }
    }

    template<typename Fn>
    constexpr auto visit(Fn &&fn) const;

    constexpr std::size_t size() const ;

    constexpr const value_t &operator[](std::size_t pos) const;
    constexpr const value_t &operator[](std::string_view) const;

    constexpr std::string_view key_at(std::size_t pos) const;


    template<typename T>
    constexpr T as() const;

    class getter_t {
    public:

        getter_t(const getter_t  &) = delete;
        getter_t &operator=(const getter_t  &) = delete;
        constexpr getter_t(const value_t &val):_val(val) {};
        template<typename T>
        constexpr operator T() const {return _val.as<T>();}
    protected:
        const value_t &_val;
    };


    constexpr getter_t get() const {
        return getter_t(*this);
    }

    constexpr explicit operator bool() const {return defined();}

    constexpr bool operator==(std::nullptr_t) const {
        return _data.type == undef || _data.type == null;
    }


    constexpr type_t type() const {
        switch (_data.type) {
            case undef: return type_t::undefined;
            case null: return type_t::null;
            case string_view:
            case string: return type_t::string;
            case array:
            case c_array: return type_t::array;
            case object:
            case c_object: return type_t:: object;
            case boolean: return type_t::boolean;
            case vint:
            case vuint:
            case vlong:
            case vulong:
            case vllong:
            case vullong:
            case d: return type_t::number;
            case list: return type_t::array;
            case number_string_view:
            case number_string: return type_t::number;
            default:
                return _str.isnum?type_t::number: type_t::string;
        }
    }

    constexpr bool defined() const {
        return _data.type != undef;
    }

    constexpr std::string_view key() const {
        if (_data.has_key)  {
            const value_t *k = this;
            --k;
            return k->as<std::string_view>();
        } else {
            return {};
        }
    }

    template<int step>
    class t_iterator_t {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = value_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        constexpr t_iterator_t():is_kv(false),pos_val(nullptr) {}
        constexpr t_iterator_t(const key_value_t *pos):is_kv(true),pos_kv(pos) {}
        constexpr t_iterator_t(const value_t *pos):is_kv(false),pos_val(pos) {}
        constexpr t_iterator_t &operator++();
        constexpr t_iterator_t &operator--();
        constexpr t_iterator_t &operator+=(int n);
        constexpr t_iterator_t &operator-=(int n);
        constexpr t_iterator_t operator++(int) {
            t_iterator_t tmp = *this; operator++(); return tmp;
        }
        constexpr t_iterator_t operator--(int) {
            t_iterator_t tmp = *this; operator--(); return tmp;
        }
        constexpr t_iterator_t operator+(int n) const;
        constexpr t_iterator_t operator-(int n) const;
        constexpr std::ptrdiff_t operator-(const t_iterator_t &other) const;
        constexpr const value_t &operator *() const ;
        constexpr std::string_view key() const;
        constexpr bool operator==(const t_iterator_t &other) const {
            return is_kv?pos_kv == other.pos_kv:pos_val == other.pos_val;
        }

    protected:

        bool is_kv;
        union {
            const value_t *pos_val;
            const key_value_t *pos_kv;
        };
    };

    using iterator_t = t_iterator_t<1>;

    constexpr bool operator==(const value_t &other) const;

    std::string to_string() const {
        return as<std::string>();
    }

    std::string stringify() const;
    static constexpr value_t parse(const std::string_view &text);

    constexpr iterator_t begin() const;
    constexpr iterator_t end() const;

protected:

    static constexpr void sort_object(shared_array_t<key_value_t> *obj);

    enum Type : unsigned char{
        undef = 32,
        null = 33,
        string_view = 35,
        string = 36,
        array = 37,
        object = 38,
        boolean = 39,
        vint = 40,
        vuint = 41,
        vlong = 42,
        vulong = 43,
        vllong = 44,
        vullong = 45,
        d = 46,
        c_array = 47,
        c_object = 48,
        list = 49,
        number_string = 50,
        number_string_view = 51
    };


    struct LocalStr {
        unsigned char size:4 = 0;
        unsigned char isnum:1 = 0;
        unsigned char empty:2 =0;
        unsigned char has_key:1 = 0;
        char str[15] = {};
    };

     struct Data{
        Type type:7;
        unsigned char has_key:1;
        std::uint32_t size;
        union {
            const char *str_view;
            shared_array_t<char> *shared_str;
            shared_array_t<value_t> *array;
            shared_array_t<key_value_t> *object;
            const value_t *c_array;
            const std::initializer_list<list_item_t> *list;
            int vint;
            unsigned int vuint;
            long vlong;
            unsigned long vulong;
            long long vllong;
            unsigned long long vullong;
            double d;
            bool b;
        };
   };

    union {
        LocalStr _str;
        Data _data;
    };

    constexpr value_t &mark_has_key() {
        _data.has_key = 1;
        return *this;
    }

    template<std::size_t Elements, std::size_t StringBytes>
    friend class value_container_t;
    friend class list_item_t;

};


struct key_value_t {
    value_t key;
    value_t value;

    constexpr bool operator==(const key_value_t &other) const {
        return key == other.key && value == other.value;
    }
};

inline constexpr value_t::value_t(const array_t &arr)
    :_data{array, 0, 0,{.array = shared_array_t<value_t>::create(arr.size(), [&](auto from, auto ){
    std::copy(arr.begin(), arr.end(), from);
})}} {}

inline constexpr value_t::value_t(const object_t &obj)
:_data{object, 0, 0,{.object = shared_array_t<key_value_t>::create(obj.size(), [&](auto from, auto ){
    std::copy(obj.begin(), obj.end(), from);
})}} {
    sort_object(_data.object);
    for (auto &x: *_data.object) x.value.mark_has_key();
}


inline constexpr value_t::~value_t() {
        switch(_data.type) {
            case number_string:
            case string: _data.shared_str->release_ref();break;
            case array: _data.array->release_ref();break;
            case object: _data.object->release_ref();break;
            default:break;
        }
}

constexpr std::size_t value_t::size() const {
    switch (_data.type) {
        case array: return std::distance(_data.array->begin(),_data.array->end());
        case object: return std::distance(_data.object->begin(),_data.object->end());
        case c_object:
        case c_array: return _data.size;
        case list: return _data.list->size();
        default: return 0;
    }
}

inline constexpr value_t undefined = {};



template<typename Fn>
inline constexpr auto value_t::visit(Fn &&fn) const {

    switch (_data.type) {
        case undef: return fn(undefined_t{});
        case null: return fn(nullptr);
        case string_view: return fn(std::string_view(_data.str_view,_data.size));
        case string: return fn(std::string_view(_data.shared_str->begin(),_data.shared_str->end()));
        case array: return fn(array_t(_data.array->begin(), _data.array->end()));
        case object: return fn(object_t(_data.object->begin(), _data.object->end()));
        case c_array: return fn(array_t(_data.c_array, _data.size));
        case c_object: return fn(object_t(reinterpret_cast<const key_value_t *>(_data.c_array), _data.size));
        case list: return fn(_data.list);
        case boolean: return fn(_data.b);
        case vint: return fn(_data.vint);
        case vuint: return fn(_data.vuint);
        case vlong: return fn(_data.vlong);
        case vulong: return fn(_data.vulong);
        case vllong: return fn(_data.vllong);
        case vullong: return fn(_data.vullong);
        case d: return fn(_data.d);
        case number_string_view: return fn(number_string_t({_data.str_view,_data.size}));
        case number_string: return fn(number_string_t({_data.shared_str->begin(),_data.shared_str->end()}));
        default:
            return _str.isnum?fn(number_string_t({_str.str, _str.size})):fn(std::string_view(_str.str, _str.size));
    }

}


template<>
struct special_conversion_t<bool, std::string_view> {
    constexpr std::optional<std::string_view> operator()(const bool &b) const {
        return b?value_t::str_true:value_t::str_false;
    }
};

template<>
struct special_conversion_t<std::nullptr_t, std::string_view> {
    constexpr std::optional<std::string_view> operator()(const std::nullptr_t &) const {
        return value_t::str_null;
    }
};


template<typename T>
inline constexpr T value_t::as() const {
    if constexpr(std::is_same_v<T, std::string>) {
        return T(visit([&](const auto &item) -> std::string {
            using X = std::decay_t<decltype(item)>;
            if constexpr(std::is_null_pointer_v<X>) {
                return std::string(str_null);
            } else if constexpr(std::is_same_v<X, bool>) {
                return std::string(item?str_true:str_false);
            } else if constexpr(std::is_constructible_v<std::string_view, X>) {
                return std::string(std::string_view(item));
            } else if constexpr(std::is_arithmetic_v<X>) {
                return std::to_string(item);
            } else if constexpr(std::is_same_v<X, array_t>) {
                if (item.empty()) return "[]";
                else return "[:"+std::to_string(item.size())+"]";
            } else if constexpr(std::is_same_v<X, object_t>) {
                if (item.empty()) return "{}";
                else return "{:"+std::to_string(item.size())+"}";
            } else {
                return std::string(str_undefined);
            }
        }));
    } else {
        return visit([&](const auto &item) -> T{
            using X = std::decay_t<decltype(item)>;
            if constexpr(!std::is_null_pointer_v<X> && std::is_constructible_v<T, X>) {
                return T(item);
            } else {
                special_conversion_t<X, T> conv;
                auto converted = conv(item);
                if (converted.has_value()) return *converted;
                return T();
            }
        });
    }
}


class list_item_t: public value_t {
public:

    using value_t::value_t;

    constexpr list_item_t(const std::initializer_list<list_item_t> &list)
        :value_t(&list) {}

    static constexpr value_t build(const value_t &v) {
        return v.visit([&](const auto &x){
            using T = std::decay_t<decltype(x)>;
            if constexpr(std::is_same_v<T, const std::initializer_list<list_item_t> *>) {
                return build_item(x);
            } else if constexpr(std::is_same_v<T, array_t>) {
                return build_item(&x);
            } else {
                return v;
            }
        });
    }

    template<typename Cont>
    static constexpr value_t build_item(const Cont *lst) noexcept {
        if (lst->size()  && std::all_of(lst->begin(), lst->end(), [&](const value_t &el){
            auto sz = el.size();
            return ((sz == 1) | (sz == 2)) && el[0].type() == type_t::string;
        })) {
            auto *kv = shared_array_t<key_value_t>::create(lst->size(),
                    [&](auto beg, auto ) {
                std::transform(lst->begin(), lst->end(), beg, [&](const value_t &el){
                    return key_value_t{el[0], build(el[1])};
                });
            });
            sort_object(kv);
            return value_t(kv);
        } else {
            std::size_t needsz = 0;
            for (const auto &x : *lst) if (x.defined()) ++needsz;
            auto arr = shared_array_t<value_t>::create(needsz,
                    [&](auto beg, auto ) {
                            for (const auto &x : *lst) if (x.defined()) {
                                *beg++ = build(x);
                            }
            });
            return value_t(arr);
        }
    }
};


constexpr const value_t &value_t::operator[](std::size_t pos) const {
    switch (_data.type) {
        case array: return _data.array->size()<=pos?undefined:_data.array->begin()[pos];
        case c_array: return _data.size <= pos?undefined:_data.c_array[pos];
        case object: return _data.object->size()<=pos?undefined:_data.object->begin()[pos].value;
        case c_object: return _data.size <= pos?undefined:_data.c_array[pos*2+1];
        case list: return _data.list->size() <= pos?undefined: _data.list->begin()[pos];
        default: return undefined;
    }
}

constexpr const value_t &value_t::operator[](std::string_view val) const {
    switch (_data.type) {
        case object: {
            key_value_t srch = {value_t(undefined_t{}, val),{}};
            auto iter = std::lower_bound(_data.object->begin(), _data.object->end(), srch, [](const key_value_t &a, const key_value_t &b){
                return a.key.as<std::string_view>() < b.key.as<std::string_view>();
            });
            if (iter == _data.object->end() || iter->key != srch.key) return undefined;
            return iter->value;
        }
        case c_object: {
            value_t srch(val);
            t_iterator_t<2> beg (_data.c_array);
            t_iterator_t<2> end (_data.c_array+_data.size*2);
            auto iter = std::lower_bound(beg, end, srch, [](const value_t &a, const value_t &b){
                auto as = a.as<std::string_view>();
                auto bs = b.as<std::string_view>();
                return as<bs;
            });
            if (iter == end || *iter != srch) return undefined;
            return (&(*iter))[1];

        }
        default: return undefined;
    }

}

constexpr std::string_view value_t::key_at(std::size_t pos) const {
    switch (_data.type) {
        case object: return _data.object->size()<=pos?std::string_view():_data.object->begin()[pos].key.as<std::string_view>();
        case c_object: return _data.size <= pos?std::string_view():_data.c_array[pos*2].as<std::string_view>();
        default: return {};
    }
}


inline constexpr value_t::value_t(const std::initializer_list<list_item_t> &lst)
    :value_t(list_item_t::build_item(&lst)) {

}


template<int step>
inline constexpr value_t::t_iterator_t<step> &value_t::t_iterator_t<step>::operator++()  {
    if (is_kv) pos_kv+=step; else pos_val+=step;
    return *this;
}
template<int step>
inline constexpr value_t::t_iterator_t<step> &value_t::t_iterator_t<step>::operator--() {
    if (is_kv) pos_kv-=step; else pos_val-=step;
    return *this;
}
template<int step>
inline constexpr value_t::t_iterator_t<step> &value_t::t_iterator_t<step>::operator+=(int n) {
    if (is_kv) pos_kv+=n*step; else pos_val+=n*step;
    return *this;
}
template<int step>
inline constexpr value_t::t_iterator_t<step> &value_t::t_iterator_t<step>::operator-=(int n){
    if (is_kv) pos_kv-=n*step; else pos_val-=n*step;
    return *this;
}

template<int step>
inline constexpr value_t::t_iterator_t<step> value_t::t_iterator_t<step>::operator+(int n) const {
    if (is_kv) return t_iterator_t<step>(pos_kv + n*step);
    else return t_iterator_t<step>(pos_val + n*step);
}
template<int step>
inline constexpr value_t::t_iterator_t<step> value_t::t_iterator_t<step>::operator-(int n) const {
    if (is_kv) return t_iterator_t<step>(pos_kv - n*step);
    else return t_iterator_t<step>(pos_val - n*step);
}
template<int step>
inline constexpr std::ptrdiff_t value_t::t_iterator_t<step>::operator-(const t_iterator_t &other) const {
    if (is_kv) return (pos_kv - other.pos_kv)/step;
    else return (pos_val - other.pos_val)/step;
}
template<int step>
inline constexpr const value_t &value_t::t_iterator_t<step>::operator *() const {
    if (is_kv) return pos_kv->value;
    else return *pos_val;
}
template<int step>
inline constexpr std::string_view value_t::t_iterator_t<step>::key() const {
    if (is_kv) return pos_kv->key.as<std::string_view>();
    else return {};
}
inline constexpr bool value_t::operator==(const value_t &other) const {
    if (type() != other.type()) return false;
    switch (type()) {
        case type_t::null:
        case type_t::undefined: return true;
        case type_t::boolean: return as<bool>() == other.as<bool>();
        case type_t::string:  return as<std::string_view>() == other.as<std::string_view>();
        case type_t::number:  return as<double>() == other.as<double>();
        case type_t::array: {
            array_t arr1 = as<array_t>();
            array_t arr2 = other.as<array_t>();
            return std::equal(arr1.begin(), arr1.end(),arr2.begin(), arr2.end());
        }
        case type_t::object: {
            object_t obj1 = as<object_t>();
            object_t obj2 = other.as<object_t>();
            return std::equal(obj1.begin(), obj1.end(),obj2.begin(), obj2.end());
        }
        default: return false;
    }
}

constexpr void value_t::sort_object(shared_array_t<key_value_t> *obj) {
    std::sort(obj->begin(), obj->end(),[&](const key_value_t &a, const key_value_t &b){
        return a.key.as<std::string_view>() < b.key.as<std::string_view>();
    });
    for (auto &x: *obj) x.key.mark_has_key();
}

inline constexpr value_t::iterator_t value_t::begin() const {
    return visit([](const auto &x) -> iterator_t{
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_t> || std::is_same_v<T, array_t>) {
            return x.data();
        } else {
            return {};
        }
    });
}
inline constexpr value_t::iterator_t value_t::end() const {
    return visit([](const auto &x) -> iterator_t{
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_t> || std::is_same_v<T, array_t>) {
            return x.data()+x.size();
        } else {
            return {};
        }
    });
}


template<std::size_t Elements, std::size_t StringBytes = 1>
class value_container_t: public value_t {
public:

    constexpr value_container_t(std::initializer_list<list_item_t> items)
        :value_container_t(value_t(items)){}

    constexpr value_container_t(value_t src) {
        std::size_t pos = 0;
        pos = expand(*this, src, pos);
        if (pos != Elements) {
            char element_too_large[1];
            std::ignore = element_too_large[pos];
        }
        pos = 0;
        pos = expand_strings(pos);
        if (pos != StringBytes) {
            char string_buffer_too_large[1];
            std::ignore = string_buffer_too_large[pos];

        }
    }

protected:

    constexpr std::size_t expand_strings(std::size_t pos) {
        for (std::size_t i = 0; i < Elements; i++) {
            if (_elements[i]._data.type == string || _elements[i]._data.type == number_string) {
                std::string_view s = _elements[i].template as<std::string_view>();
                const char *c = copy_string(pos, s);
                if (_elements[i]._data.type == string) {
                    _elements[i] = value_t(std::string_view(c, s.size()));
                } else {
                    _elements[i] = value_t(number_string_t({c, s.size()}));
                }
                if (_elements[i]._data.has_key) {
                    _elements[i].mark_has_key();
                }

            }
        }
        return pos;
    }

    constexpr std::size_t expand(value_t &trg, const value_t &src, std::size_t pos) {
        switch (src.type()) {
            case type_t::object: {
                auto kvlist = src.as<object_t>();
                std::size_t sz = kvlist.size();
                value_t *beg = _elements+pos;
                value_t *iter = beg;
                pos+=sz*2;
                for (std::size_t i = 0; i < sz ; ++i) {
                    pos=expand(*iter, kvlist[i].key, pos);
                    ++iter;
                    pos=expand(*iter, kvlist[i].value, pos);
                    iter->mark_has_key();
                    ++iter;
                }
                trg = value_t(beg, sz, true);
            } break;
            case type_t::array: {
                std::size_t sz = src.size();
                value_t *beg = _elements+pos;
                value_t *iter = beg;
                pos+=sz;
                for (std::size_t i = 0; i < sz ; ++i) {
                    pos=expand(*iter, src[i], pos);
                    ++iter;
                }
                trg = value_t(beg, sz, false);
            } break;
            case type_t::number:
            default:
                trg = src;
                break;
        }
        return pos;
    }




    constexpr const char *copy_string(std::size_t &pos, std::string_view text) {
        if (text.empty()) return nullptr;
        char *beg = _buffer+pos;
        pos += text.size()+1;
        auto end = std::copy(text.begin(), text.end(), beg);
        *end = 0;
        return beg;
    }



    value_t _elements[Elements];
    char _buffer[StringBytes] = {};


};

//clang complains for undefined function
template value_t list_item_t::build_item<std::span<value_t const> >(std::span<value_t const> const*);
//clang complains for undefined function
template std::string json20::value_t::as<std::string>() const;

template class value_t::t_iterator_t<1>;
template class value_t::t_iterator_t<2>;;
}