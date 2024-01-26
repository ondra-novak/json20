#pragma once

#include "spec_conv.h"
#include "shared_array.h"
#include "base64.h"
#include <span>
#include <tuple>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace json20 {

using binary_string_view_t = std::basic_string_view<unsigned char>;

using binary_string_t = std::basic_string<unsigned char>;

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

///Some tags for strings
enum class string_type_t {
    ///utf_8 string
    utf_8,
    ///binary string
    binary,
    ///number as string
    number
};

namespace _details {
    class list_item_t;
}

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


///Declaration of JSON value
class value_t {
public:

    ///Text representation of null value
    static constexpr std::string_view str_null = "null";
    ///Text representation of true value
    static constexpr std::string_view str_true = "true";
    ///Text representation of false value
    static constexpr std::string_view str_false = "false";
    ///Text representation of undefined value
    static constexpr std::string_view str_undefined = "undefined";

    ///constructs undefined value
    constexpr value_t():_data{undef_default, 0, 0, {}} {}
    constexpr ~value_t();

    ///construct copy
    constexpr value_t(const value_t &other);
    ///move constructor
    constexpr value_t( value_t &&other);
    ///assign by copy
    constexpr value_t &operator=(const value_t &other);
    ///assign by move
    constexpr value_t &operator=( value_t &&other);

    ///construct null
    constexpr value_t(std::nullptr_t):_data{null,0,0,{}} {}
    ///construct from c-text
    constexpr value_t(const char *text):value_t(std::string_view(text)) {}
    ///construct bool
    constexpr value_t(bool b):_data{boolean,0,0, {.b = b}} {}
    ///construct int
    constexpr value_t(int val):_data{vint,0,0,{.vint = val}} {}
    ///construct unsigned int number
    constexpr value_t(unsigned int val):_data{vuint,0,0,{.vuint = val}} {}
    ///construct long number
    constexpr value_t(long val):_data{vlong,0,0,{.vlong = val}} {}
    ///construct unsigned long number
    constexpr value_t(unsigned long val):_data{vulong,0,0,{.vulong = val}} {}
    ///construct long long number
    constexpr value_t(long long val):_data{vllong,0,0,{.vllong = val}} {}
    ///construct unsigned long long number
    constexpr value_t(unsigned long long val):_data{vullong,0,0,{.vullong = val}} {}
    ///construct double value
    constexpr value_t(double val):_data{dbl,0,0,{.d = val}} {}
    ///construct structured value (array or object)
    /**
     * @param lst if there is array of pairs, where first of the pair is string, an
     * object is constructed, otherwise, an array is constructed
     */
    constexpr value_t(const std::initializer_list<_details::list_item_t> &lst);
    ///construct array
    constexpr value_t(const array_t &arr);
    ///construct object
    constexpr value_t(const object_t &obj);
    ///construct from shared array
    /**
     * @param arr pointer to array as shared_array object. The ownership is transfered to
     * the value_t
     */
    explicit constexpr value_t(shared_array_t<value_t> *arr):_data{array, 0, 0, {.array = arr}} {}
    ///construct from shared array
    /**
     * @param arr pointer to array as shared_array object. The ownership is transfered to
     * the value_t
     */
    explicit constexpr value_t(shared_array_t<key_value_t> *obj):_data{object, 0, 0, {.object = obj}} {
        sort_object(_data.object);
    }
    ///construct string from shared string buffer
    /**
     *
     * @param str string buffer - pointer ownership is transfered
     * @param number specify true if the buffer is number as string
     */

    constexpr value_t(shared_array_t<char> *str, bool number):_data{number?number_string:string, 0, 0, {.shared_str = str}} {}

    ///construct binary string from shared string buffer
    /**
     *
     * @param str string buffer - pointer ownership is transfered
     */
    constexpr value_t(shared_array_t<unsigned char> *str):_data{binary_string, 0, 0, {.shared_bin_str = str}} {}

    ///Construct string
    /**
     * @param text text to construct
     * @note constexpr version doesn't allocate the string. If you need this, you must allocate
     * shared array bufer and construct it with pointer to that buffer. Non-constexpr version
     * copies the string into the object (uses small string buffer, or allocates the buffer)
     *
     */

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

    ///Constructs number string
    /**
     *
     * @param text text to construct
     * @note constexpr version doesn't allocate the string. If you need this, you must allocate
     * shared array bufer and construct it with pointer to that buffer. Non-constexpr version
     * copies the string into the object (uses small string buffer, or allocates the buffer)
     *
     */
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

    constexpr value_t(const binary_string_view_t &bin_text) {
        if (std::is_constant_evaluated()) {
            std::construct_at(&_data, Data{binary_string_view, 0,static_cast<std::uint32_t>(bin_text.size()), {.bin_str_view = bin_text.data()}});
        } else {
            _data = {binary_string,0, 0,{.shared_bin_str = shared_array_t<unsigned char>::create(bin_text.size(),[&](unsigned char *beg, unsigned char *){
                std::copy(bin_text.begin(), bin_text.end(), beg);
            })}};
        }
    }


    ///Construct array or object as view to preallocated buffer of items
    /**
     * @param elements pointer to elements
     * @param sz count of items. For object, you specify count of pairs (so if need to divide count of elements by two)
     * @param is_object specify true, if the pointer points to an object (where elements are in format string, value, string, value)
     */
    constexpr value_t(const value_t *elements, std::size_t sz, bool is_object)
        :_data({is_object?c_object:c_array,0,static_cast<std::uint32_t>(sz),{.c_array = elements}}) {}


    ///Construct default value for specified type
    /**
     * @param type type to be constructed
     * @retval undefined type was type_t:undefined
     * @retval null type was type_t::null
     * @retval false type was type_t::boolean
     * @retval "" type was type_t::string
     * @retval 0 type was type_t::number
     * @retval [] type was type_t::array
     * @retval {} type was type_t::object
     */
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

    ///Construct string view
    /**
     * The object value_t is able to hold reference to preallocated string. You
     * must keep this string valid for whole lifetime of the object
     * @param txt preallocated string
     * @return value
     */
    static constexpr value_t create_string_view(std::string_view txt) {
        value_t out;
        out._data.type = string_view;
        out._data.size = txt.size();
        out._data.str_view = txt.data();
        return out;
    }

    ///Visit the value
    /**
     * @param fn lambda function (template function). The function is called with type
     * representing internal value.
     * @return return value of the function
     *
     * @note the function is called with number value of any supported type. If the value
     * is undefined, the function is called with undefined_t. For null the function
     * receives std::nullptr_t. If the value contains a string, the function is called
     * with std::string_view. If the value is number_string, the function is called
     * with number_string_t. If the value is array or object, the function is called
     * with array_t or object_t.
     */
    template<typename Fn>
    constexpr auto visit(Fn &&fn) const;

    ///Returns count of elements in the container
    /**
     * @return count of elements in the container. Returns 0 if the value is not container
     */
    constexpr std::size_t size() const ;

    ///Access the contained value
    /**
     * @param pos zero base index.
     * @return value at index. If the index is out of range, undefined is returned
     *
     * @note you can use this operator for object too.
     */
    constexpr const value_t &operator[](std::size_t pos) const;
    ///Access the value of the obj addressed by a key
    /**
     * @param  key value
     * @return value under the key, if the key doesn't exists, returns undefined
     */
    constexpr const value_t &operator[](std::string_view) const;

    ///Access the key at index
    /**
     * @param pos returns key of the item at given index (for object only)
     * @return key at the index
     */

    constexpr std::string_view key_at(std::size_t pos) const;


    ///Retrieve value as type
    /**
     * @tparam T required type. The value must be convertible to the T. The T must
     * be constructible by default constructor
     * @return value converted to T, if not convertible, returns T()
     */
    template<typename T>
    constexpr T as() const;

    ///Tests, whether the internal value is convertible to T
    /**
     * @tparam T required type
     * @retval true value can be converted to T (there is conversion possible)
     * @retval false no conversion is possible
     */
    template<typename T>
    constexpr bool contains() const;

    ///Helper getter class
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

    ///Get value and use getter to automatically convert to required type
    /**
     * @return a getter which contains the value. The getter can be automatically converted
     * to any supported type, because it defines operator of conversion.
     */
    constexpr getter_t get() const {
        return getter_t(*this);
    }

    ///Determines whether value is defined
    /**
     * @retval true value is defined
     * @retval false value is undefined
     */
    constexpr explicit operator bool() const {return defined();}

    ///Determines whether object contains meaningful value
    /**
     * @param must be null
     * @retval true value is null or undefined
     * @retval false value is defined and it is not null
     */
    constexpr bool operator==(std::nullptr_t) const {
        return _data.type == undef || _data.type == undef_default || _data.type == null;
    }

    ///Retrieves type of value
    /**
     * @return standard JSON type of the value
     */
    constexpr type_t type() const {
        switch (_data.type) {
            case undef_default:
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
            case dbl: return type_t::number;
            case list: return type_t::array;
            case number_string_view:
            case number_string: return type_t::number;
            default:
                return _str.isnum?type_t::number: type_t::string;
        }
    }

    ///Determines whether value is defined
    /**
     * @retval true is defined
     * @retval false not defined
     */
    constexpr bool defined() const {
        return _data.type != undef && _data.type != undef_default;
    }

    ///Retrieves a key of the value
    /**
     * The value must be retrieved from an object. You need to retrieve the value as
     * reference - not as a copy
     * @return
     */
    constexpr std::string_view key() const {
        if (_data.has_key)  {
            const value_t *k = this;
            --k;
            return k->as<std::string_view>();
        } else {
            return {};
        }
    }

    ///Iterator - generic
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
        constexpr t_iterator_t &operator+=(difference_type n);
        constexpr t_iterator_t &operator-=(difference_type n);
        constexpr t_iterator_t operator++(int) {
            t_iterator_t tmp = *this; operator++(); return tmp;
        }
        constexpr t_iterator_t operator--(int) {
            t_iterator_t tmp = *this; operator--(); return tmp;
        }
        constexpr t_iterator_t operator+(difference_type n) const;
        constexpr t_iterator_t operator-(difference_type n) const;
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

    ///iterator
    using iterator_t = t_iterator_t<1>;

    ///compare values
    constexpr bool operator==(const value_t &other) const;

    ///convert to string
    /**
     * @return string representation of the value
     * @note the function doesn't work for containers.
     */
    std::string to_string() const {
        return as<std::string>();
    }

    ///Convert value to JSON string
    /**
     * @return string contains JSON representation
     * @note requires serializer.h
     */
    std::string to_json() const;
    ///Parse string as JSON
    /**
     * @param text string contains JSON
     * @return value of parsed JSON.
     * @exception parse_error_exception_t when string is not valid JSON
     *
     * @note requires parser.h
     */
    static constexpr value_t from_json(const std::string_view &text);

    ///first element of array or object
    constexpr iterator_t begin() const;
    ///last element of array or object
    constexpr iterator_t end() const;

    ///set field to new value (object)
    /**
     * @param key key
     * @param value value
     *
     * If the current value is not object, it is converted to object
     */
    void set(std::string_view key, value_t value);
    ///Sets fields
    /**
     * @param list list of fields     *
     */
    template<int N>
    void set(const std::pair<std::string_view, value_t> (&list)[N]);


    ///Merge two objects key by key
    template<typename ConflictSolver>
    value_t merge_objects(const value_t &val, ConflictSolver &&fn) const;
    value_t merge_objects(const value_t &val) const;
    value_t merge_objects_recursive(const value_t &val) const;





protected:

    explicit constexpr value_t(const std::initializer_list<_details::list_item_t> *lst):_data{list, 0,0,{.list = lst}} {}

    static constexpr void sort_object(shared_array_t<key_value_t> *obj);

    enum Type : unsigned char{
        ///undefined value
        undef = 32,
        ///undefined value constructed by default constructor
        undef_default = 33,
        ///null value
        null,
        ///string view  (str_view)
        string_view,
        ///string allocated/shared
        string,
        ///array allocated/shared
        array,
        ///object allocated/shared
        object,
        ///boolean value
        boolean,
        ///signed int
        vint,
        ///unsigned int
        vuint,
        ///signed long
        vlong ,
        ///unsigned long
        vulong ,
        ///signed long long
        vllong ,
        ///unsigned long long
        vullong,
        ///double
        dbl,
        ///constant array pointer
        c_array,
        ///object array - pointer to array, interleaved key-value
        c_object,
        ///pointer to initializer list (internal)
        list,
        ///string (allocated) as number
        number_string,
        ///string view as number
        number_string_view,
        ///binary string
        binary_string,
        ///binary string view
        binary_string_view

    };


    struct LocalStr {
        ///size of local string (0-15)
        unsigned char size:4 = 0;
        ///1 - if string is number
        unsigned char isnum:1 = 0;
        ///not used
        unsigned char empty:2 =0;
        ///1 - this value has key at index [-1]
        unsigned char has_key:1 = 0;
        ///the space of small string
        char str[15] = {};
    };

     struct Data{
        ///type of data (7 bits)
        Type type:7;
        ///1 - this value has key at index [-1]
        unsigned char has_key:1;
        ///value to store a size for some variants
        std::uint32_t size;
        union {
            ///pointer to statically allocated string
            const char *str_view;
            ///pointer to statically allocated binary string
            const unsigned char *bin_str_view;
            ///pointer to shared string buffer
            shared_array_t<char> *shared_str;
            ///pointer to shared string buffer
            shared_array_t<unsigned char> *shared_bin_str;
            ///pointer to shared array
            shared_array_t<value_t> *array;
            ///pointer to shared object
            shared_array_t<key_value_t> *object;
            ///pointer to statically allocated array
            const value_t *c_array;
            ///pointer to initializer list
            const std::initializer_list<_details::list_item_t> *list;
            ///number
            int vint;
            ///number
            unsigned int vuint;
            ///number
            long vlong;
            ///number
            unsigned long vulong;
            ///number
            long long vllong;
            ///number
            unsigned long long vullong;
            ///number
            double d;
            ///boolean
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
    friend class _details::list_item_t;

    constexpr bool is_small_string() const {
        return _data.type < undef;
    }

    template<typename Fn>
    constexpr void visit_dynamic(Fn &&fn) {
        switch(_data.type) {
            case number_string:
            case string: fn(_data.shared_str);break;
            case binary_string: fn(_data.shared_bin_str);break;
            case array: fn(_data.array);break;
            case object: fn(_data.object);break;
            default:break;
        }

    }

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


inline constexpr value_t::value_t(const value_t &other) {
    //small string
    if (other.is_small_string()) {
        //just copy _str
        std::construct_at(&_str, other._str);
        _str.has_key = 0;
    } else {
        //copy _dat
        std::construct_at(&_data, other._data);
        //reset has_key, because copy has no key
        _data.has_key = 0;
        //
        visit_dynamic([](auto ptr){ptr->add_ref();});
    }
}

inline constexpr value_t::value_t( value_t &&other) {
    if (other._data.type < undef) {
        std::construct_at(&_str, other._str);
        _str.has_key = 0;
    } else {
        std::construct_at(&_data, other._data);
        other._data.type = undef;
        _data.has_key = 0;
    }
}

inline constexpr value_t &value_t::operator=(const value_t &other) {
    if (this != &other) {
        std::destroy_at(this);
        std::construct_at(this, other);
    }
    return *this;
}

inline constexpr value_t &value_t::operator=( value_t &&other) {
    if ( this != &other) {
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
    }
    return *this;
}


inline constexpr value_t::~value_t() {
    visit_dynamic([](auto ptr){
        ptr->release_ref();
    });
}

inline constexpr std::size_t value_t::size() const {
    switch (_data.type) {
        case array: return std::distance(_data.array->begin(),_data.array->end());
        case object: return std::distance(_data.object->begin(),_data.object->end());
        case c_object:
        case c_array: return _data.size;
        case list: return _data.list->size();
        default: return 0;
    }
}

inline constexpr value_t undefined = type_t::undefined;



template<typename Fn>
inline constexpr auto value_t::visit(Fn &&fn) const {

    switch (_data.type) {
        case undef_default:
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
        case dbl: return fn(_data.d);
        case number_string_view: return fn(number_string_t({_data.str_view,_data.size}));
        case number_string: return fn(number_string_t({_data.shared_str->begin(),_data.shared_str->end()}));
        case binary_string_view: return fn(binary_string_view_t({_data.bin_str_view,_data.size}));
        case binary_string: return fn(binary_string_view_t({_data.shared_bin_str->begin(),_data.shared_bin_str->end()}));
        default:
            return _str.isnum?fn(number_string_t({_str.str, _str.size})):fn(std::string_view(_str.str, _str.size));
    }

}


template<>
struct value_conversion_t<std::string_view> {
    constexpr std::string_view operator()(bool b) const {
        return b?value_t::str_true:value_t::str_false;
    }
    constexpr std::string_view operator()(std::nullptr_t) const {
        return {};
    }
};


template<>
struct value_conversion_t<std::string> {
    std::string operator()(bool b) const {
        return std::string(b?value_t::str_true:value_t::str_false);
    }
    std::string operator()(std::span<const value_t> span) const {
        return "<array.size="+std::to_string(span.size())+">";
    }
    std::string operator()(std::span<const key_value_t> span) const {
        return "<object.size="+std::to_string(span.size())+">";
    }
    template<number_t T>
    std::string operator()(T n) const {
        return std::to_string(n);
    }
    std::string operator()(const binary_string_view_t binstr) const;
    std::string operator()(std::nullptr_t) const {return {};}
};

template<number_t T>
struct value_conversion_t<T>{
    constexpr T operator()(std::nullptr_t) const {
        return T();
    }
    constexpr T operator()(const std::string_view &v) const {
        number_string_t nv(v);
        return T(nv);
    }
};

template<>
struct value_conversion_t<binary_string_t> {
    binary_string_t operator()(std::nullptr_t) const {return {};}
    binary_string_t operator()(const std::string_view &v) const;
};




template<typename T>
inline constexpr T value_t::as() const {
    return visit([&](const auto &item) -> T{
        using X = std::decay_t<decltype(item)>;
        if constexpr(!std::is_null_pointer_v<X> && std::is_constructible_v<T, X>) {
            return T(item);
        } else if constexpr (std::invocable<value_conversion_t<T>, X>) {
            value_conversion_t<T> conv;
            return conv(item);
        } else {
            return T();
        }
    });
}

template<typename T>
inline constexpr bool value_t::contains() const {
    return visit([&](const auto &item) -> bool{
        using X = std::decay_t<decltype(item)>;
        if constexpr(!std::is_null_pointer_v<X> && std::is_constructible_v<T, X>) {
            return true;
        } else if constexpr (std::invocable<value_conversion_t<T>, X>) {
            return true;
        } else {
            return false;
        }
    });
}


namespace _details {

class list_item_t: public value_t {
public:

    using value_t::value_t;

    constexpr list_item_t(const std::initializer_list<list_item_t> &list)
        :value_t(&list) {}

    static constexpr value_t build(const value_t &v) {
        switch (v._data.type) {
            case undef_default: return type_t::array;
            case list: return build_item(v._data.list);
            default: return v;
        }
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
            for (const auto &x : *lst) if (x._data.type != undef) ++needsz;
            auto arr = shared_array_t<value_t>::create(needsz,
                    [&](auto beg, auto ) {
                            for (const auto &x : *lst) if (x._data.type != undef) {
                                *beg++ = build(x);
                            }
            });
            return value_t(arr);
        }
    }
};

}

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
            key_value_t srch = {value_t::create_string_view(val),{}};
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


inline constexpr value_t::value_t(const std::initializer_list<_details::list_item_t> &lst)
    :value_t(_details::list_item_t::build_item(&lst)) {

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
inline constexpr value_t::t_iterator_t<step> &value_t::t_iterator_t<step>::operator+=(difference_type n) {
    if (is_kv) pos_kv+=n*step; else pos_val+=n*step;
    return *this;
}
template<int step>
inline constexpr value_t::t_iterator_t<step> &value_t::t_iterator_t<step>::operator-=(difference_type n){
    if (is_kv) pos_kv-=n*step; else pos_val-=n*step;
    return *this;
}

template<int step>
inline constexpr value_t::t_iterator_t<step> value_t::t_iterator_t<step>::operator+(difference_type n) const {
    if (is_kv) return t_iterator_t<step>(pos_kv + n*step);
    else return t_iterator_t<step>(pos_val + n*step);
}
template<int step>
inline constexpr value_t::t_iterator_t<step> value_t::t_iterator_t<step>::operator-(difference_type n) const {
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

///Allows to store constexpr structured value
/**
 * @tparam Elements count of elements. This value can't be calculated by the compiler. If the
 * value is not exact, the compilation fails. However you can let compiler to show correct value
 * in error diagnostic message. If you enter high value, the compiler complaina
 * at 'element_too_large' and shows a number which is correct in this case, so you can
 * update the argument to successful compilation
 *
 * @tparam StringBytes count of bytes for strings. This value can't be calculated by the compiler.
 * value is not exact, the compilation fails. However you can let compiler to show correct value
 * in error diagnostic message. If you enter high value, the compiler complains
 * at 'string_buffer_too_large' and shows a number which is correct in this case, so you can
 * update the argument to successful compilation
 *
 * The class inherits value_t so it can be later used as value_t
 */
template<std::size_t Elements, std::size_t StringBytes = 1>
class value_container_t: public value_t {
public:

    constexpr value_container_t(std::initializer_list<_details::list_item_t> items)
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
        if (pos+16 < StringBytes) {
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

template<typename ConflictSolver>
inline value_t value_t::merge_objects(const value_t &val,ConflictSolver &&fn) const {
    if (val.type() != type_t::object || type() != type_t::object) return val;
    const object_t &a = as<object_t>();
    const object_t &b = val.as<object_t>();
    const key_value_t *resbeg, *resend;
    auto res = shared_array_t<key_value_t>::create(size()+val.size(), [&](auto beg, auto){
        resbeg = beg;
        auto ia = a.begin();
        auto ea = a.end();
        auto ib = b.begin();
        auto eb = b.end();
        while (ia != ea && ib != eb) {
            auto sa = ia->key.as<std::string_view>();
            auto sb = ib->key.as<std::string_view>();
            if (sa<sb) {
                *beg= *ia;
                ++ia;
            } else if (sa > sb) {
                *beg= *ib;
                ++ib;
            } else {
                beg->key = ia->key;
                beg->value = fn(ia->value, ib->value);
                ++ia;
                ++ib;
            }
            if (beg->value.defined()) {
                ++beg;
            }
        }
        while (ia != ea) {
            *beg = *ia;
            ++ia;
            ++beg;
        }
        while (ib != eb) {
            *beg = *ib;
           ++ib;
           ++beg;
        }
        resend = beg;
    });
    res->trunc(resend - resbeg);
    return value_t(res);
}

inline value_t value_t::merge_objects(const value_t &val) const {
    return merge_objects(val, [](const value_t &, const value_t &b) {
        return b;
    });
}

inline value_t value_t::merge_objects_recursive(const value_t &val) const {
    return merge_objects(val, [](const value_t &a, const value_t &b) {
        return a.merge_objects_recursive(b);
    });
}



inline void value_t::set(std::string_view key, value_t value) {
    set({{key, value}});

}

template<int N>
void value_t::set(const std::pair<std::string_view, value_t> (&list)[N]) {
    if constexpr(N <= 0) return ;
    const std::pair<std::string_view , value_t> * sortindex[N];
    int i = 0;
    for (const auto &x: list) sortindex[++i] = &x;
    std::sort(std::begin(sortindex), std::end(sortindex), [&](const auto *a, const auto *b){
        return a->first < b->first;
    });
    value_t data[N * 2];
    for (int i = 0; i < N; ++i) {
        data[i*2] = sortindex[i]->first;
        data[i*2+1] = sortindex[i]->second;
    }
    value_t obj(data, N, true);
    (*this) = this->merge_objects(obj);
}



inline std::string value_conversion_t<std::string>::operator()(const binary_string_view_t binstr) const {
    std::string out;
    base64.encode(binstr.begin(), binstr.end(), std::back_inserter(out));
    return out;
}

inline binary_string_t value_conversion_t<binary_string_t>::operator()(const std::string_view &v) const {
    binary_string_t out;
    base64.decode(v.begin(), v.end(), std::back_inserter(out));
    return out;
}


//clang complains for undefined function
template value_t _details::list_item_t::build_item<std::span<value_t const> >(std::span<value_t const> const*) noexcept;
//clang complains for undefined function
template std::string json20::value_t::as<std::string>() const;

template class value_t::t_iterator_t<1>;
template class value_t::t_iterator_t<2>;;



}

