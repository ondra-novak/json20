#pragma once

#include "number_string.h"
#include "shared_array.h"
#include "base64.h"
#include <span>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <iterator>

namespace json20 {

///undefined type - represents undefined value (monostate)
struct undefined_t {};
template<typename To>
struct value_conversion_t {};


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
enum class string_type {
    ///utf_8 string
    utf_8,
    ///binary string
    binary,
    ///number as string
    number
};

namespace _details {
    class list_item;
}

class value_t;
template<unsigned int N> class array;
template<unsigned int N> class object;

using array_view = std::span<const value_t>;
using object_view = std::span<const key_value_t>;

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
    constexpr value_t():_data{undef, 0, 0, {.str_view  = nullptr}} {}
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

    template<int N>
    constexpr value_t(const char (&text)[N]):_data{string_view, 0, N-1, {.str_view = text}} {}

    ///construct structured value (array or object)
    /**
     * @param lst if there is array of pairs, where first of the pair is string, an
     * object is constructed, otherwise, an array is constructed
     */
    constexpr value_t(const std::initializer_list<_details::list_item> &lst);
    ///construct array
    constexpr value_t(const array_view &arr);
    ///construct object
    constexpr value_t(const object_view &obj);
    ///construct array from constexpr array
    template<unsigned int N>
    constexpr value_t(const value_t (&arr)[N]);
    ///construct object from constexpr object
    template<unsigned int N>
    constexpr value_t(const key_value_t (&kv)[N]);
///construct from shared array
    /**
     * @param arr pointer to array as shared_array object. The ownership is transfered to
     * the value_t
     */
    explicit constexpr value_t(shared_array_t<value_t> *arr):_data{shared_array, 0, 0, {.shared_array = arr}} {}
    ///construct from shared array
    /**
     * @param arr pointer to array as shared_array object. The ownership is transfered to
     * the value_t
     */
    explicit constexpr value_t(shared_array_t<key_value_t> *obj);
    ///construct string from shared string buffer
    /**
     *
     * @param str string buffer - pointer ownership is transfered
     * @param number specify true if the buffer is number as string
     */

    constexpr value_t(shared_array_t<char> *str, bool number):_data{number?num_string:string, 0, 0, {.shared_str = str}} {}

    ///construct binary string from shared string buffer
    /**
     *
     * @param str string buffer - pointer ownership is transfered
     */
    constexpr value_t(shared_array_t<unsigned char> *str):_data{binary_string, 0, 0, {.shared_bin_str = str}} {}


    template<std::forward_iterator Iter, std::invocable<std::iter_value_t<Iter> > Fn>
    constexpr value_t(Iter begin, Iter end, Fn &&fn) {
        using RetV = std::decay_t<std::invoke_result_t<Fn, decltype(*begin)> >;
        using T = std::conditional_t<std::is_same_v<RetV, key_value_t>, RetV,
                std::conditional_t<std::is_same_v<RetV, char>, RetV, value_t> >;
        auto arr = shared_array_t<T>::create(std::distance(begin,end), [&](auto beg, auto){
            std::transform(begin, end, beg, std::forward<Fn>(fn));
        });
        if constexpr(std::is_same_v<T, key_value_t>) {
           std::construct_at(&_data, Data{shared_object,0,0,{.shared_object = arr}});
        } else if constexpr(std::is_same_v<T, char>) {
           std::construct_at(&_data, Data{string,0,0,{.shared_str= arr}});
        } else {
           std::construct_at(&_data, Data{shared_array,0,0,{.shared_array = arr}});
        }
    }

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
    constexpr value_t(const number_string &text) {
        if (std::is_constant_evaluated()) {
            std::construct_at(&_data, Data{num_string_view, 0,static_cast<std::uint32_t>(text.size()), {text.data()}});
        } else {
            if (text.size() < 16) {
                std::construct_at(&_str);
                _str.size = static_cast<unsigned char>(text.size());
                _str.isnum = 1;
                std::copy(text.begin(), text.end(), _str.str);
            } else {
                _data = {num_string,0, 0,{.shared_str = shared_array_t<char>::create(text.size(),[&](char *beg, char *){
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
            case type_t::array: std::construct_at(&_data, Data{const_array,0,0,{.const_array = nullptr}}); break;
            case type_t::object: std::construct_at(&_data, Data{const_object,0,0,{.const_object = nullptr}}); break;
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
        out._data.size = static_cast<std::uint32_t>(txt.size());
        out._data.str_view = txt.data();
        return out;
    }

    ///Construct array view - used int constexpr evaluation
    static constexpr value_t create_array_view(const value_t *array, std::size_t count) {
        value_t out;
        out._data.type = const_array;
        out._data.size = static_cast<std::uint32_t>(count);
        out._data.const_array = array;
        return out;
    }
    ///Construct object view - used int constexpr evaluation
    /** @note result is not const expression - it cannot be used in constexpr evaluation
     *
     * @param object pointer to object, stored in array, as key, value interleaved
     * @param count count of pairs
     * @return value (not constexpr - reinterpret_cast is used)
     */
    static constexpr value_t create_object_view(const value_t *object, std::size_t count) {
        value_t out;
        out._data.type = const_object;
        out._data.size = static_cast<std::uint32_t>(count);
        out._data.const_array = object;
        return out;
    }

    ///Construct object view - used int constexpr evaluation
    /** @note result is not const expression - it cannot be used in constexpr evaluation
     *
     * @param object pointer to object, stored in array, as key, value interleaved
     * @param count count of pairs
     * @return value (not constexpr - reinterpret_cast is used)
     */
    static constexpr value_t create_object_view(const key_value_t *object, std::size_t count) {
        value_t out;
        out._data.type = const_object;
        out._data.size = static_cast<std::uint32_t>(count);
        out._data.const_object = object;
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
     * with number_string. If the value is array or object, the function is called
     * with array_t or object_t.
     */
    template<typename Fn>
    constexpr decltype(auto) visit(Fn &&fn) const;

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
        return _data.type == undef || _data.type == null;
    }

    ///Retrieves type of value
    /**
     * @return standard JSON type of the value
     */
    constexpr type_t type() const {
        switch (_data.type) {
            case undef: return type_t::undefined;
            case null: return type_t::null;
            case string_view:
            case string: return type_t::string;
            case shared_array:
            case const_array: return type_t::array;
            case shared_object:
            case const_object: return type_t:: object;
            case boolean: return type_t::boolean;
            case vint:
            case vuint:
            case vlong:
            case vulong:
            case vllong:
            case vullong:
            case dbl: return type_t::number;
            case num_string_view:
            case num_string: return type_t::number;
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
        return _data.type != undef;
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

    class iterator_t {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = value_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        constexpr iterator_t():is_kv(false),pos_val(nullptr) {}
        constexpr iterator_t(const key_value_t *pos):is_kv(true),pos_kv(pos) {}
        constexpr iterator_t(const value_t *pos):is_kv(false),pos_val(pos) {}
        constexpr iterator_t &operator++();
        constexpr iterator_t &operator--();
        constexpr iterator_t &operator+=(difference_type n);
        constexpr iterator_t &operator-=(difference_type n);
        constexpr iterator_t operator++(int) {
            iterator_t tmp = *this; operator++(); return tmp;
        }
        constexpr iterator_t operator--(int) {
            iterator_t tmp = *this; operator--(); return tmp;
        }
        constexpr iterator_t operator+(difference_type n) const;
        constexpr iterator_t operator-(difference_type n) const;
        constexpr std::ptrdiff_t operator-(const iterator_t &other) const;
        constexpr const value_t &operator *() const ;
        constexpr std::string_view key() const;
        constexpr bool operator==(const iterator_t &other) const {
            return is_kv?pos_kv == other.pos_kv:pos_val == other.pos_val;
        }

    protected:

        bool is_kv;
        union {
            const value_t *pos_val;
            const key_value_t *pos_kv;
        };
    };


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


    ///Convert value to JSON string
    /**
     * @param buffer a vector used to store actual string data
     * @return string view containing the JSON string
     *
     * @note the buffer is not cleared, so you can use this to append a data
     * to it, however the return value contains just generated JSON
     * @note requires serializer.h
     * @note it is constexpr
     */
    constexpr std::string_view to_json(std::vector<char> &buffer) const;

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



    template<typename Iter>
    static constexpr void sort_object(Iter beg, Iter end);


protected:



    enum Type : unsigned char{
        ///undefined value
        undef = 32,
        ///null value
        null,
        ///string view  (str_view)
        string_view,
        ///string allocated/shared
        string,
        ///array allocated/shared
        shared_array,
        ///object allocated/shared
        shared_object,
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
        const_array,
        ///object array - pointer to key_value array (sorted)
        const_object,
        ///string (allocated) as number
        num_string,
        ///string view as number
        num_string_view,
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
            shared_array_t<value_t> *shared_array;
            ///pointer to shared object
            shared_array_t<key_value_t> *shared_object;
            ///pointer to statically allocated array
            const value_t *const_array;
            ///pointer to statically allocated array
            const key_value_t *const_object;
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
    template<unsigned int N> friend class object;
    friend class _details::list_item;

    constexpr bool is_small_string() const {
        return _data.type < undef;
    }

    template<typename Fn>
    constexpr void visit_dynamic(Fn &&fn) {
        switch(_data.type) {
            case num_string:
            case string: fn(_data.shared_str);break;
            case binary_string: fn(_data.shared_bin_str);break;
            case shared_array: fn(_data.shared_array);break;
            case shared_object: fn(_data.shared_object);break;
            default:break;
        }

    }

};

class key_t {
public:

    constexpr key_t():_name("") {}
    constexpr key_t(std::string_view name):_name(name) {}
    template<int N>
    constexpr key_t(const char (&name)[N]):_name(name) {}

    constexpr operator std::string_view() const {
        return _name.as<std::string_view>();
    }
    constexpr operator std::string() const {
        return _name.as<std::string>();
    }
    constexpr operator const value_t &() const {
        return _name;
    }
    constexpr operator value_t &&() && {
        return std::move(_name);
    }


    constexpr std::string_view str() const {
        return _name.as<std::string_view>();
    }

    constexpr static key_t from_value(const value_t &value) {
        return key_t(value);
    }
    constexpr static key_t from_value(value_t &&value) {
        return key_t(std::move(value));
    }

    ///compare keys - use template to solve ambiguity
    template<std::same_as<key_t> Key>
    constexpr bool operator==(const Key &other) const {
        return str() == other.str();
    }
    ///compare keys - use template to solve ambiguity
    template<std::same_as<key_t> Key>
    constexpr int operator<=>(const Key &other) const {
        return str().compare(other.str());
    }



protected:
    constexpr key_t(const value_t &val):_name(val) {}

    value_t _name;
};

///contains pair key and value
struct key_value_t {
    ///contains key
    key_t key = {};
    ///contains value
    value_t value = {};
};


template<typename Iter>
constexpr void value_t::sort_object(Iter beg, Iter end) {
    std::sort(beg, end,[&](const key_value_t &a, const key_value_t &b){
        return a.key < b.key;
    });
    std::for_each(beg, end, [](key_value_t &x){
        x.value.mark_has_key();
    });
}


inline constexpr value_t::value_t(const array_view &arr)
    :_data{shared_array, 0, 0,{.shared_array = shared_array_t<value_t>::create(arr.size(), [&](auto from, auto ){
    std::copy(arr.begin(), arr.end(), from);
})}} {}

inline constexpr value_t::value_t(const object_view &obj)
:_data{shared_object, 0, 0,{.shared_object = shared_array_t<key_value_t>::create(obj.size(), [&](auto from, auto ){
    std::copy(obj.begin(), obj.end(), from);
})}} {
    sort_object(_data.shared_object->begin(), _data.shared_object->end());
}

constexpr value_t::value_t(shared_array_t<key_value_t> *obj):_data{shared_object, 0, 0, {.shared_object = obj}} {
    sort_object(_data.shared_object->begin(), _data.shared_object->end());
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
        case shared_array: return std::distance(_data.shared_array->begin(),_data.shared_array->end());
        case shared_object: return std::distance(_data.shared_object->begin(),_data.shared_object->end());
        case const_object:
        case const_array: return _data.size;
        default: return 0;
    }
}

inline constexpr value_t undefined = type_t::undefined;



template<typename Fn>
inline constexpr decltype(auto) value_t::visit(Fn &&fn) const {

    switch (_data.type) {
        case undef: return fn(undefined_t{});
        case null: return fn(nullptr);
        case string_view: return fn(std::string_view(_data.str_view,_data.size));
        case string: return fn(std::string_view(_data.shared_str->begin(),_data.shared_str->end()));
        case shared_array: return fn(array_view(_data.shared_array->begin(), _data.shared_array->end()));
        case shared_object: return fn(object_view(_data.shared_object->begin(), _data.shared_object->end()));
        case const_array: return fn(array_view(_data.const_array, _data.size));
        case const_object: return fn(object_view(_data.const_object, _data.size));
        case boolean: return fn(_data.b);
        case vint: return fn(_data.vint);
        case vuint: return fn(_data.vuint);
        case vlong: return fn(_data.vlong);
        case vulong: return fn(_data.vulong);
        case vllong: return fn(_data.vllong);
        case vullong: return fn(_data.vullong);
        case dbl: return fn(_data.d);
        case num_string_view: return fn(number_string({_data.str_view,_data.size}));
        case num_string: return fn(number_string({_data.shared_str->begin(),_data.shared_str->end()}));
        case binary_string_view: return fn(binary_string_view_t({_data.bin_str_view,_data.size}));
        case binary_string: return fn(binary_string_view_t({_data.shared_bin_str->begin(),_data.shared_bin_str->end()}));
        default:
            return _str.isnum?fn(number_string({_str.str, _str.size})):fn(std::string_view(_str.str, _str.size));
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
        number_string nv(v);
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

class list_item {
public:

    enum class Type {
        empty,
        element,
        list
    };

    constexpr list_item():_type(Type::empty) {}
    template<std::convertible_to<value_t> T>
    constexpr list_item(T && val):_type(Type::element),_val(std::forward<T>(val)) {}
    constexpr list_item(std::initializer_list<list_item> list):_type(Type::list),_list(std::move(list)) {}
    constexpr list_item(list_item &&other):_type(other._type) {
        switch(_type) {
            case Type::element: std::construct_at(&_val, std::move(other._val));break;
            case Type::list: std::construct_at(&_list, std::move(other._list));break;
            default:break;
        }
    }
    constexpr ~list_item() {
        switch(_type) {
            case Type::element: std::destroy_at(&_val);break;
            case Type::list: std::destroy_at(&_list);break;
            default:break;
        }
    }

    constexpr value_t build() const  {
        switch (_type) {
            case Type::empty: return value_t(type_t::array);
            case Type::element:return _val;
            case Type::list: break;
        }

        if (_list.size()>0 && std::all_of(_list.begin(), _list.end(), [&](const list_item &item){
            return  (item._type == Type::list
              && item._list.size() == 2
              && item._list.begin()->_type == Type::element
              && item._list.begin()->_val.type() == type_t::string);
        })) {
            auto *kv = shared_array_t<key_value_t>::create(_list.size(), [&](auto beg, auto ) {
                std::transform(_list.begin(), _list.end(), beg, [&](const list_item &el){
                    auto i1 = el._list.begin();
                    auto i2 = i1;
                    ++i2;
                    return key_value_t{key_t::from_value(i1->_val), i2->build()};
                });
            });
            return value_t(kv);
        } else {
            std::size_t cnt = 0;
            for (const auto &x: _list) {
                if (x._type != Type::element || x._val.defined()) ++cnt;
            }
            if (cnt == 0) return value_t(type_t::array);
            auto *arr = shared_array_t<value_t>::create(cnt, [&](auto beg, auto){
                for (const auto &x: _list) {
                    if (x._type != Type::element || x._val.defined()) {
                        *beg++=x.build();
                    }
                }
            });
            return value_t(arr);
        }
    }


protected:
    Type _type;
    union {
        value_t _val;
        std::initializer_list<list_item> _list;
    };

};

}

constexpr const value_t &value_t::operator[](std::size_t pos) const {
    switch (_data.type) {
        case shared_array: return _data.shared_array->size()<=pos?undefined:_data.shared_array->begin()[pos];
        case const_array: return _data.size <= pos?undefined:_data.const_array[pos];
        case shared_object: return _data.shared_object->size()<=pos?undefined:_data.shared_object->begin()[pos].value;
        case const_object: return _data.size <= pos?undefined:_data.const_object[pos].value;
        default: return undefined;
    }
}

constexpr const value_t &value_t::operator[](std::string_view val) const {
    return visit([&](const auto &x) -> const value_t & {
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view>) {
            auto iter = std::lower_bound(x.begin(), x.end(), val, [](const auto &a, const auto &b){
                using A = std::decay_t<decltype(a)>;
                if constexpr(std::is_same_v<A, key_value_t>) {
                    return a.key < b;
                } else {
                    return a < b.key;
                }
            });
            if (iter == x.end() || iter->key != val) return undefined;
            return iter->value;
        } else {
            return undefined;
        }
    });

}

constexpr std::string_view value_t::key_at(std::size_t pos) const {
    return visit([&](const auto &x) -> std::string_view {
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view>) {
            if (pos >= x.size()) return {};
            return x[pos].key;
        } else {
            return {};;
        }
    });
}


inline constexpr value_t::value_t(const std::initializer_list<_details::list_item> &lst)
    :value_t(_details::list_item(lst).build()) {

}


inline constexpr value_t::iterator_t &value_t::iterator_t::operator++()  {
    if (is_kv) pos_kv+=1; else pos_val+=1;
    return *this;
}
inline constexpr value_t::iterator_t &value_t::iterator_t::operator--() {
    if (is_kv) pos_kv-=1; else pos_val-=1;
    return *this;
}
inline constexpr value_t::iterator_t &value_t::iterator_t::operator+=(difference_type n) {
    if (is_kv) pos_kv+=n; else pos_val+=n;
    return *this;
}
inline constexpr value_t::iterator_t &value_t::iterator_t::operator-=(difference_type n){
    if (is_kv) pos_kv-=n; else pos_val-=n;
    return *this;
}

inline constexpr value_t::iterator_t value_t::iterator_t::operator+(difference_type n) const {
    if (is_kv) return iterator_t(pos_kv + n);
    else return iterator_t(pos_val + n);
}
inline constexpr value_t::iterator_t value_t::iterator_t::operator-(difference_type n) const {
    if (is_kv) return iterator_t(pos_kv - n);
    else return iterator_t(pos_val - n);
}
inline constexpr std::ptrdiff_t value_t::iterator_t::operator-(const iterator_t &other) const {
    if (is_kv) return (pos_kv - other.pos_kv);
    else return (pos_val - other.pos_val);
}
inline constexpr const value_t &value_t::iterator_t::operator *() const {
    if (is_kv) return pos_kv->value;
    else return *pos_val;
}
inline constexpr std::string_view value_t::iterator_t::key() const {
    if (is_kv) return pos_kv->key;
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
            array_view arr1 = as<array_view>();
            array_view arr2 = other.as<array_view>();
            return std::equal(arr1.begin(), arr1.end(),arr2.begin(), arr2.end());
        }
        case type_t::object: {
            object_view obj1 = as<object_view>();
            object_view obj2 = other.as<object_view>();
            return std::equal(obj1.begin(), obj1.end(),obj2.begin(), obj2.end(),
                [](const key_value_t &a,const key_value_t &b){return a.key == b.key && a.value == b.value;});
        }
        default: return false;
    }
}

inline constexpr value_t::iterator_t value_t::begin() const {
    return visit([](const auto &x) -> iterator_t{
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view> || std::is_same_v<T, array_view>) {
            return x.data();
        } else {
            return {};
        }
    });
}
inline constexpr value_t::iterator_t value_t::end() const {
    return visit([](const auto &x) -> iterator_t{
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view> || std::is_same_v<T, array_view>) {
            return x.data()+x.size();
        } else {
            return {};
        }
    });
}

template<unsigned int N>
class array: public value_t {
public:

    constexpr array(const std::initializer_list<value_t> &items):
        value_t(value_t::create_array_view(_items, N)) {
        std::copy(std::begin(items), std::end(items), std::begin(_items));
    }
    constexpr array(const value_t (&items)[N]):
        value_t(value_t::create_array_view(_items, N)) {
        std::copy(std::begin(items), std::end(items), std::begin(_items));
    }

protected:
    value_t _items[N] = {};

};


template<unsigned int N>
class object: public value_t {
public:

    constexpr object(const std::initializer_list<std::pair<std::string_view,value_t> > &items):
        value_t(value_t::create_object_view(_items, N)) {
        std::transform(std::begin(items), std::end(items), std::begin(_items),[](const auto &p){
            return key_value_t{p.first, p.second};
        });
        value_t::sort_object(std::begin(_items), std::end(_items));
    }
    constexpr object(const std::pair<std::string_view,value_t> (&items)[N]):
        value_t(value_t::create_object_view(_items, N)) {
        std::transform(std::begin(items), std::end(items), std::begin(_items),[](const auto &p){
            return key_value_t{p.first, p.second};
        });
        value_t::sort_object(std::begin(_items), std::end(_items));
    }

protected:
    value_t _test = {};
    key_value_t _items[N] = {};
};


template<unsigned int N>
array(const value_t (&v)[N]) -> array<N>;

template<unsigned int N>
object(const std::pair<std::string_view, value_t> (&)[N]) -> object<N>;


namespace _details {

    inline constexpr unsigned int calc_space_fn(const value_t &v) {
        return v.visit([&](auto item){
            using T = std::decay_t<decltype(item)>;
            if constexpr(std::is_same_v<T, array_view>) {
                unsigned int cnt = 1;
                for (const auto &x: item) cnt += calc_space_fn(x);
                return cnt;
            } else if constexpr(std::is_same_v<T, object_view>) {
                unsigned int cnt = 1;
                for (const auto &x: item) cnt += 1+calc_space_fn(x.value);
                return cnt;
            } else  {
                return 1U;
            }
        });
    }

    template<auto source_fn>
    inline constexpr unsigned int calc_space = ([](){
        return calc_space_fn(source_fn());
    })();


}

template<auto N>
class structured;

template<unsigned int N>
class structured_n {
public:

    constexpr void reserved_size_mismatch(unsigned int n) {
        throw n;
    }

    constexpr structured_n(const value_t &val) {
        unsigned int pos = build(1, val, _items);
        if (pos != N) {
            reserved_size_mismatch(pos);
        }
    }

    constexpr structured_n(const std::initializer_list<_details::list_item> &list):structured_n(value_t(list)) {}

    constexpr operator const value_t &() const {
        return _items[0];
    }
    constexpr operator value_t() const {
        return _items[0];
    }

protected:
    value_t _items[N];

    constexpr unsigned int build(unsigned int pos, const value_t &val, value_t *trg) {
        return val.visit([&](const auto &item){
            using T = std::decay_t<decltype(item)>;
            if constexpr(std::is_same_v<T, array_view>) {
                value_t *wr = _items+pos;
                *trg = value_t::create_array_view(wr, item.size());
                pos+=item.size();
                std::for_each(item.begin(), item.end(), [&](const value_t &v){
                    pos = build(pos, v, wr);
                    ++wr;
                });
            }
            else if constexpr(std::is_same_v<T, object_view>) {
                value_t *wr = _items+pos;
                *trg = value_t::create_object_view(wr, item.size());
                pos+=item.size()*2;
                std::for_each(item.begin(), item.end(), [&](const key_value_t &v){
                    *wr = v.key;
                    ++wr;
                    pos = build(pos, v.value, wr);
                    ++wr;
                });
            }else {
                *trg = val;
            }
            return pos;
        });
    }
};

template<auto N>
requires std::is_integral_v<decltype(N)>
class structured<N>: public structured_n<N> {
public:
    using structured_n<N>::structured_n;
};

template<auto N>
requires std::is_invocable_v<decltype(N)>
class structured<N>: public structured_n<_details::calc_space<N> > {
public:
   constexpr structured():structured_n<_details::calc_space<N> >(N()) {}
};

template<unsigned int N>
constexpr value_t::value_t(const value_t (&arr)[N])
    :_data({const_array,0,static_cast<std::uint32_t>(N),{.const_array = arr}}) {
}

template<unsigned int N>
constexpr value_t::value_t(const key_value_t (&obj)[N])
    :_data({const_object,0,static_cast<std::uint32_t>(N),{.const_object = obj}}) {
}


template<typename ConflictSolver>
inline value_t value_t::merge_objects(const value_t &val,ConflictSolver &&fn) const {
    if (val.type() != type_t::object || type() != type_t::object) return val;
    const object_view &a = as<object_view>();
    const object_view &b = val.as<object_view>();
    const key_value_t *resbeg, *resend;
    auto res = shared_array_t<key_value_t>::create(size()+val.size(), [&](auto beg, auto){
        resbeg = beg;
        auto ia = a.begin();
        auto ea = a.end();
        auto ib = b.begin();
        auto eb = b.end();
        while (ia != ea && ib != eb) {
            std::string_view sa = ia->key;
            std::string_view sb = ib->key;
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
    key_value_t tmp[N];
    std::transform(std::begin(list), std::end(list), std::begin(tmp), [](const auto &p)->key_value_t{
        return {p.first, p.second};
    });
    value_t obj = value_t::create_object_view(tmp, N);
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
template std::string json20::value_t::as<std::string>() const;

using type = type_t;
using value = value_t;
using key_value = key_value_t;

}

