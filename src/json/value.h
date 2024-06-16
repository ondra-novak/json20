#pragma once

#include "base64.h"
#include "common.h"
#include "number_string.h"
#include "shared_array.h"
#include "object_view.h"

namespace JSON20_NAMESPACE_NAME {

///undefined type - represents undefined value (monostate)
struct undefined_t {};
template<typename To>
struct value_conversion_t {};


using binary_string_view_t = std::basic_string_view<unsigned char>;

using binary_string_t = std::basic_string<unsigned char>;


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

template<unsigned int N> struct placeholder_t {
    static constexpr unsigned int position = N;
    static_assert(N > 0);
};

template<unsigned int N>
static constexpr placeholder_t<N> placeholder = {};
template<unsigned int N>
static constexpr placeholder_t<N> __ = {};

struct is_array_t {};
static constexpr is_array_t is_array{};
struct is_object_t {};
static constexpr is_object_t is_object{};

class value_t;
using key_value_t = std::pair<std::string_view, value_t>;
template<unsigned int N> class array_t;
template<unsigned int N> class object_t;

using array_view = std::span<const value_t>;
using object_view = object_view_gen<value_t>;

struct placeholder_view {
    unsigned int position;
    constexpr bool operator==(const placeholder_view &other) const {
        return position == other.position;
    }
};

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

    template<unsigned int N>
    constexpr value_t(placeholder_t<N> pos):_data{placeholder, 0, 0, {.placeholder_pos = pos.position}} {}

    ///construct structured value (array or object)
    /**
     * @param lst if there is array of pairs, where first of the pair is string, an
     * object is constructed, otherwise, an array is constructed
     */
    constexpr value_t(std::initializer_list<_details::list_item> lst);
    ///construct array
    constexpr value_t(const array_view &arr);
    ///construct object
    constexpr value_t(const object_view &obj);
    ///construct from shared array
    /**
     * @param arr pointer to array as shared_array object. The ownership is transfered to
     * the value_t
     */
    constexpr value_t(shared_array_t<value_t> *arr, is_array_t):_data{shared_array, 0, 0, {.shared_array = arr}} {}
    ///construct object from shared array
    /**
     * @param arr pointer to array as shared_array object. The array must contain
     * keys and values in interleave order. Keys are allowed only strings
     *
     */
    constexpr value_t(shared_array_t<value_t> *arr, is_object_t):_data{shared_object, 0, 0, {.shared_array = arr}} {
        sort_object(_data.shared_array->begin(), _data.shared_array->end());
    }

    ///construct view on array
    /**
     * @param view pointer to a first item of an unspecified array
     * @param count count of items in array
     *
     * @note as it is view, you must ensure, that target object will not
     * be destroyed prior end of lifetime of this element
     */
    constexpr value_t(const value_t *view, std::uint32_t count, is_array_t):_data{const_array, 0, count, {.const_array = view}} {}
    ///construct view on array
    /**
     * @param view pointer to a first item of an unspecified array, must contains
     * keys and values in interleaved order. Keys must be represented as strings.
     * @param count count of items in array (keys and values in total, so number
     * is always even
     *
     * @note as it is view, you must ensure, that target object will not
     * be destroyed prior end of lifetime of this element
     */
    constexpr value_t(const value_t *view, std::uint32_t count, is_object_t):_data{const_object, 0, count, {.const_array = view}} {}

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

    ///constrcut string view
    /**
     * @param view pointer to memory where string is constructed
     * @param count count of characters
     * @note doesn't copy the string
     */
    constexpr value_t(const char *view, std::uint32_t count, bool number):_data{number?num_string_view:string_view,0, count, {.str_view = view}} {}

    constexpr value_t(const unsigned char *view, std::uint32_t count):_data{binary_string_view,0, count, {.bin_str_view = view}} {}


    template<std::forward_iterator Iter, std::invocable<std::iter_value_t<Iter> > Fn>
    constexpr value_t(Iter beg, Iter end, Fn &&fn):value_t(transform(beg, end, std::forward<Fn>(fn))) {}

    template<std::forward_iterator Iter>
    constexpr value_t(Iter beg, Iter end):value_t(transform(beg, end, [](const auto &x){return x;})) {}

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

    value_t(std::string text):value_t(std::string_view(text)) {}


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
            case type_t::object: std::construct_at(&_data, Data{const_object,0,0,{.const_array= nullptr}}); break;
        }
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
            case placeholder: return type_t::undefined;
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

        constexpr iterator_t() = default;
        constexpr iterator_t(const value_t *pos , unsigned int shift):pos(pos), shift(shift) {}
        constexpr iterator_t &operator++() {pos+=1<<shift;return *this;}
        constexpr iterator_t &operator--() {pos-=1<<shift;return *this;}
        constexpr iterator_t &operator+=(difference_type n) {pos+=n<<shift;return *this;}
        constexpr iterator_t &operator-=(difference_type n) {pos-=n<<shift;return *this;}
        constexpr iterator_t operator++(int) {
            iterator_t tmp = *this; operator++(); return tmp;
        }
        constexpr iterator_t operator--(int) {
            iterator_t tmp = *this; operator--(); return tmp;
        }
        constexpr iterator_t operator+(difference_type n) const {return {pos+(n<<shift), shift};}
        constexpr iterator_t operator-(difference_type n) const {return {pos+(n<<shift), shift};}
        constexpr std::ptrdiff_t operator-(const iterator_t &other) const {
            return (pos - other.pos) >> shift;
        }
        constexpr const value_t &operator *() const {return pos[shift];}
        constexpr std::string_view key() const {return pos[0].as<std::string_view>();}
        constexpr bool operator==(const iterator_t &other) const {
            return pos == other.pos;
        }

    protected:

        const value_t *pos = nullptr;
        unsigned int shift = 0;
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

    ///determines, whether other object is copy of current object
    /**
     * This can be used to detect changes in containers. If the
     * value is not copy of original value, something has changed, and
     * the new value contains an update.
     *
     * @param other to test
     * @retval true other argument is copy of this object. It means, that
     * containers shares the same content. For non containers, standard
     * comparison is used.
     */
    constexpr bool is_copy_of(const value_t &other) const;




    template<typename Iter>
    static constexpr void sort_object(Iter beg, Iter end);


    constexpr bool is_placeholder() const {
        return _data.type == placeholder;
    }

    constexpr unsigned int get_placeholder_pos() const {
        return _data.placeholder_pos;
    }

    constexpr value_t operator()(const std::initializer_list<value_t> &list) const;


    struct compact_info {
        std::size_t element_count = 0;
        std::size_t string_buffer_size = 0;
        std::size_t binary_buffer_size = 0;
    };

    struct compact_pointers {
        value_t *elements;
        char *string_buffer;
        unsigned char *binary_buffer;
    };

    constexpr void compact(compact_pointers &ptrs);
    constexpr void compact(compact_pointers &&ptrs) {compact(ptrs);}
    constexpr compact_info compact_calc_space() const;

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
        binary_string_view,
        ///placeholder
        placeholder,
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
            ///pointer to statically allocated array
            const value_t *const_array;
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
            ///placeholder_position
            unsigned int placeholder_pos;
            ///pointer relative
            int ptr_rel;
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
    template<unsigned int N> friend class object_t;
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
            case shared_object:
            case shared_array: fn(_data.shared_array);break;
            default:break;
        }

    }

    //returns undefined, if no replacement was made
    constexpr value_t replace_placeholder(const std::initializer_list<value_t> &list) const;

    template<bool object, typename Iter>
    value_t find_and_replace_placeholders(Iter beg, Iter end,const std::initializer_list<value_t> &list) const;

    template<std::forward_iterator Iter, std::invocable<std::iter_value_t<Iter> > Fn>
    requires std::convertible_to<std::invoke_result_t<Fn, std::iter_value_t<Iter> >, value_t>
    static value_t transform(Iter beg, Iter end, Fn &&fn) {
        auto count = std::distance(beg, end);
        return value_t(shared_array_t<value_t>::create(count, [&](auto from, auto){
            for (auto iter = beg; iter != end; ++iter) {
                *from++ = fn(*iter);
            }
        }),is_array);
    }

    template<std::forward_iterator Iter, std::invocable<std::iter_value_t<Iter> > Fn>
    requires std::convertible_to<std::invoke_result_t<Fn, std::iter_value_t<Iter> >, key_value_t>
    static value_t transform(Iter beg, Iter end, Fn &&fn) {
        auto count = std::distance(beg, end)*2;
        return value_t(shared_array_t<value_t>::create(count, [&](auto from, auto){
            for (auto iter = beg; iter != end; ++iter) {
                auto [key, val] = fn(*iter);
                *from++ = std::string_view(key);
                *from++ = std::move(val);
            }
        }),is_object);
    }

};



template<typename Iter>
constexpr void value_t::sort_object(Iter beg, Iter end) {
    pair_iterator pbeg(beg);
    pair_iterator pend(end);
    std::sort(pbeg, pend,[&](const auto &a, const auto &b){
        return a.first().template as<std::string_view>() < b.first().template as<std::string_view>();
    });
    std::for_each(pbeg, pbeg, [](auto &x){
        x.second().mark_has_key();
    });
}


inline constexpr value_t::value_t(const array_view &arr)
    :_data{shared_array, 0, 0,{.shared_array = shared_array_t<value_t>::create(arr.size(), [&](auto from, auto ){
    std::copy(arr.begin(), arr.end(), from);
})}} {}

inline constexpr value_t::value_t(const object_view &obj)
:_data{shared_object, 0, 0,{.shared_array = shared_array_t<value_t>::create(obj.size()*2, [&](auto from, auto ){
    std::for_each(obj.begin(), obj.end(), [&](object_view::iterator::reference val){
        *from++ = val.key;
        *from++ = val.value;
    });
})}} {
    sort_object(_data.shared_array->begin(), _data.shared_array->end());
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
        case shared_array: return _data.shared_array->size();
        case shared_object: return _data.shared_array->size()/2;
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
        case shared_object: return fn(object_view(_data.shared_array->begin(), _data.shared_array->end()));
        case const_array: return fn(array_view(_data.const_array, _data.size));
        case const_object: return fn(object_view(_data.const_array, _data.size));
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
        case placeholder: return fn(placeholder_view{_data.placeholder_pos});
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
    std::string operator()(array_view span) const {
        return "<array.size="+std::to_string(span.size())+">";
    }
    std::string operator()(object_view span) const {
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
            auto *kv = shared_array_t<value_t>::create(_list.size()*2, [&](auto beg, auto ) {
                for (const auto &x: _list) {
                    auto i1 = x._list.begin();
                    auto i2 = i1;
                    ++i2;
                    *beg = i1->_val.template as<std::string_view>();
                    ++beg;
                    *beg = i2->build();
                    ++beg;
                }
            });
            return value_t(kv, is_object);
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
            return value_t(arr, is_array);
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
        case shared_object: return _data.shared_array->size()<=pos*2?undefined:_data.shared_array->begin()[pos+1];
        case const_object: return _data.size <= pos*2?undefined:_data.const_array[pos*2];
        default: return undefined;
    }
}

constexpr const value_t &value_t::operator[](std::string_view val) const {
    return visit([&](const auto &x) -> const value_t & {
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view>) {
            auto beg = x.begin();
            auto end = x.end();
            auto iter = std::lower_bound(beg, end, val, [](const auto &a, const auto &b){
                using A = std::decay_t<decltype(a)>;
                if constexpr(std::is_same_v<A, std::string_view>) {
                    return a < b.key;
                } else {
                    return a.key < b;
                }
            });
            if (iter == end || iter->key != val) return undefined;
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


inline constexpr value_t::value_t(std::initializer_list<_details::list_item> lst)
    :value_t(_details::list_item(lst).build()) {

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
            return std::equal(obj1.begin(), obj1.end(),obj2.begin(), obj2.end());
        }
        default: return false;
    }
}

inline constexpr value_t::iterator_t value_t::begin() const {
    return visit([](const auto &x) -> iterator_t{
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view>) {
            return {x.data(),1};
        } else if constexpr(std::is_same_v<T, array_view>) {
            return {x.data(),0};
        } else {
            return {};
        }
    });
}
inline constexpr value_t::iterator_t value_t::end() const {
    return visit([](const auto &x) -> iterator_t{
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, object_view>) {
            return {x.data()+x.size()*2,1};
        } else if constexpr(std::is_same_v<T, array_view>) {
            return {x.data()+x.size(),0};
        } else {
            return {};
        }
    });
}

template<unsigned int N>
class array_t: public value_t {
public:

    constexpr array_t(const std::initializer_list<value_t> &items):
        value_t(_items, N, is_array) {
        std::copy(std::begin(items), std::end(items), std::begin(_items));
    }
    constexpr array_t(const value_t (&items)[N]):
        value_t(_items, N, is_array) {
        std::copy(std::begin(items), std::end(items), std::begin(_items));
    }

protected:
    value_t _items[N] = {};

};


template<unsigned int N>
class object_t: public value_t {
public:

    constexpr object_t(const std::initializer_list<std::pair<std::string_view,value_t> > &items):
        value_t(_items, N*2, is_object) {
        do_copy(items);
    }

    constexpr object_t(const std::pair<std::string_view,value_t> (&items)[N]):
        value_t(_items, N*2, is_object) {
        do_copy(items);
    }

protected:
    value_t _items[N*2] = {};

    template<typename X>
    constexpr void do_copy(const X &items) {
        unsigned int pos = 0;
        for (const auto &[key, value]: items) {
            _items[pos] = key;
            _items[pos+1] = value;
            pos+=2;
        }
        value_t::sort_object(std::begin(_items), std::end(_items));
    }

};

template<unsigned int N>
array_t(const value_t (&v)[N]) -> array_t<N>;

template<unsigned int N>
object_t(const std::pair<std::string_view, value_t> (&)[N]) -> object_t<N>;



template<std::invocable<> Fn>
class structured_t : public value_t{
public:

    static_assert(std::is_same_v<std::invoke_result_t<Fn>, value_t>);

    constexpr structured_t(Fn fn):value_t(fn()) {
        this->compact(compact_pointers{_items,_buffer,_bbuffer});
    }

protected:
    static constexpr Fn _c_instance = {};
    static constexpr value_t::compact_info _cinfo = _c_instance().compact_calc_space();
    value_t _items[_cinfo.element_count] = {};
    char _buffer[std::max<std::size_t>(1,_cinfo.string_buffer_size)] = {};
    unsigned char _bbuffer[std::max<std::size_t>(1,_cinfo.binary_buffer_size)] = {};
};

template<typename ConflictSolver>
inline value_t value_t::merge_objects(const value_t &val,ConflictSolver &&fn) const {
    if (val.type() != type_t::object || type() != type_t::object) return val;
    const object_view &a = as<object_view>();
    const object_view &b = val.as<object_view>();
    const value_t *resbeg, *resend;
    auto res = shared_array_t<value_t>::create((size()+val.size())*2, [&](auto beg, auto){
        resbeg = beg;
        auto ia = a.begin();
        auto ea = a.end();
        auto ib = b.begin();
        auto eb = b.end();
        while (ia != ea && ib != eb) {
            const auto &va = *ia;
            const auto &vb = *ib;
            if (va.key<vb.key) {
                *beg++ = va.key;
                *beg++= va.value;
                ++ia;
            } else if (va.key > vb.key) {
                if (vb.value.defined()) {
                    *beg++ = vb.key;
                    *beg++ = vb.value;
                }
                ++ib;
            } else {
                if (vb.value.defined()) {
                    *beg++ = vb.key;
                    *beg++ = fn(va.value,vb.value);
                }
                ++ia;
                ++ib;
            }
        }
        while (ia != ea) {
            const auto &va = *ia;
            *beg++ = va.key;
            *beg++ = va.value;
            ++ia;
        }
        while (ib != eb) {
            const auto &vb = *ib;
            if (vb.value.defined()) {
                *beg++ = vb.key;
                *beg++ = vb.value;
            }
           ++ib;
        }
        resend = beg;
    });
    res->trunc(resend - resbeg);
    return value_t(res, is_object);
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
    value_t tmp[N*2];
    for (int i = 0; i < N; i++) {
        tmp[i*2] = list[i].first;
        tmp[i*2+1] = list[i].second;
    }
    value_t::sort_object(std::begin(tmp), std::end(tmp));
    value_t obj(tmp, N*2,is_object);
    (*this) = this->merge_objects(obj);
}

inline constexpr bool value_t::is_copy_of(const value_t &other) const {
    return visit([&](const auto &x){
        using T = std::decay_t<decltype(x)>;
        return other.visit([&](const auto &y){
            using U = std::decay_t<decltype(y)>;
            if constexpr(!std::is_same_v<T, U>) {
                return false;
            } else if constexpr(std::is_same_v<T, object_view>) {
                return x.begin() == y.begin();
            } else if constexpr(std::is_same_v<T, array_view>) {
                return x.begin() == y.begin();
            } else if constexpr(std::is_same_v<T, undefined_t>){
                return true;
            } else {
                return x == y;
            }
        });
    });
}

inline constexpr value_t value_t::replace_placeholder(const std::initializer_list<value_t> &list) const {
    switch (_data.type){
        case placeholder:
            if (_data.placeholder_pos >= list.size()) return nullptr;
            else return *(list.begin()+_data.placeholder_pos);
        case shared_array:
            return find_and_replace_placeholders<false>(_data.shared_array->begin(), _data.shared_array->end(), list);
        case const_array:
            return find_and_replace_placeholders<false>(_data.const_array, _data.const_array+_data.size, list);
        case shared_object:
            return find_and_replace_placeholders<true>(_data.shared_array->begin(), _data.shared_array->end(), list);
        case const_object:
            return find_and_replace_placeholders<true>(_data.const_array, _data.const_array+_data.size, list);
        default:
            return {};
    }

}

template<bool object, typename Iter>
inline value_t value_t::find_and_replace_placeholders(Iter beg, Iter end,const std::initializer_list<value_t> &list) const {
    value_t first_replace;

    Iter fnd = std::find_if(beg, end, [&](const value_t &val){
        first_replace = val.replace_placeholder(list);
        return first_replace.defined();
    });

    if (fnd != end) {
        auto ret = shared_array_t<value_t>::create(std::distance(beg,end), [&](auto from, auto){
            from = std::copy(beg, fnd, from);
            *from++ = std::move(first_replace);
            ++fnd;
            from = std::transform(fnd, end, from, [&](const value_t &v){
                auto tmp = v.replace_placeholder(list);
                if (tmp.defined()) return tmp;
                else return v;
            });
        });
        if constexpr(object) {
            return value_t(ret, is_object);
        } else {
            return value_t(ret, is_array);
        }
    }
    return {};
}


inline constexpr value_t value_t::operator()(const std::initializer_list<value_t> &list) const {
    value_t res = replace_placeholder(list);
    if (res.defined()) return res;
    else return *this;
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

inline constexpr void value_t::compact(compact_pointers &ptrs) {
    visit([&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, std::string_view>) {
            if (ptrs.string_buffer) {
                auto b = ptrs.string_buffer;
                ptrs.string_buffer = std::copy(x.begin(), x.end(), ptrs.string_buffer);
                *ptrs.string_buffer = 0;
                std::string_view newstr(b, ptrs.string_buffer);
                ++ptrs.string_buffer;
                *this = value_t(newstr.data(), newstr.size(), false);
            }
        } else if constexpr(std::is_same_v<T, binary_string_view_t>) {
            if (ptrs.binary_buffer) {
                auto b = ptrs.binary_buffer;
                ptrs.binary_buffer = std::copy(x.begin(), x.end(), ptrs.binary_buffer);
                binary_string_view_t newstr(b, ptrs.binary_buffer);
                *this = value_t(newstr.data(), newstr.size());
            }
        } else if constexpr(std::is_same_v<T, number_string>) {
            if (ptrs.string_buffer) {
                auto b = ptrs.string_buffer;
                ptrs.string_buffer = std::copy(x.begin(), x.end(), ptrs.string_buffer);
                *ptrs.string_buffer = 0;
                std::string_view newstr(b, ptrs.string_buffer);
                ++ptrs.string_buffer;
                *this = value_t(newstr.data(), newstr.size(), true);
            }
        } else if constexpr(std::is_same_v<T, object_view>) {
            auto start = ptrs.elements;
            auto pos = ptrs.elements;
            auto sz = x.size()*2;
            ptrs.elements += sz;
            for (object_view::iterator::reference v: x) {
                *pos = v.key;
                pos->compact(ptrs);
                pos++;
                *pos = v.value;
                pos->compact(ptrs);
                pos++;
            }
            *this = value_t(start, sz,is_object);
        } else if constexpr(std::is_same_v<T, array_view>) {
            auto start = ptrs.elements;
            auto pos = ptrs.elements;
            auto sz = x.size();
            ptrs.elements += sz;
            for (array_view::iterator::reference v: x) {
                *pos = v;
                pos->compact(ptrs);
                pos++;
            }
            *this = value_t(start, sz, is_array);
        }
    });
}

inline constexpr value_t::compact_info value_t::compact_calc_space() const {
    return visit([&](const auto &x) {
        compact_info nfo;
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, std::string_view> || std::is_same_v<T, number_string>) {
                nfo.string_buffer_size+=x.size()+1;
        } else if constexpr(std::is_same_v<T, binary_string_view_t>) {
                nfo.binary_buffer_size+=x.size();
        } else if constexpr(std::is_same_v<T, object_view>) {
            for (object_view::iterator::reference v: x) {
                auto n = v.value.compact_calc_space();
                nfo.string_buffer_size+=v.key.size()+1+n.string_buffer_size;
                nfo.element_count+=2+n.element_count;
                nfo.binary_buffer_size+=n.binary_buffer_size;
            }
        } else if constexpr(std::is_same_v<T, array_view>) {
            for (array_view::iterator::reference v: x) {
                auto n = v.compact_calc_space();
                nfo.string_buffer_size+=n.string_buffer_size;
                nfo.element_count+=1+n.element_count;
                nfo.binary_buffer_size+=n.binary_buffer_size;
            }
        }
        return nfo;
    });
}



//clang complains for undefined function
template std::string value_t::as<std::string>() const;

using type = type_t;
using value = value_t;

template<std::invocable<> Fn>
constexpr auto structured(Fn &&fn) {
    return structured_t<Fn>(std::forward<Fn>(fn));
}
template<unsigned int N>
constexpr array_t<N> array(const value_t (&n)[N]) {return n;}
template<unsigned int N>
constexpr auto object(const std::pair<std::string_view,value_t> (&n)[N]) {return object_t<N>(n);}

}

