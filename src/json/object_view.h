#pragma once

#include "common.h"

namespace JSON20_NAMESPACE_NAME {


///Allows to view object containers - to see keys and values
template<typename Value>
class object_view_gen {
public:

    struct value_type {
        std::string_view key;
        const Value &value;
        constexpr value_type(const std::string_view &key,const Value &value):key(key),value(value) {}
        constexpr bool operator==(const value_type &other) const{
            return key == other.key && value == other.value;
        }
    };


    constexpr object_view_gen() = default;
    constexpr object_view_gen(const Value *items, std::size_t count):_items(items), _count(count) {}
    constexpr object_view_gen(const Value *beg, const Value *end):_items(beg), _count(end - beg) {}

    constexpr std::size_t size() const {return _count/2;}

    constexpr value_type operator[](std::size_t pos) const {
        return {_items[pos * 2].template as<std::string_view>(),
            _items[pos*2+1]};
    }


    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = std::add_const_t<const object_view_gen::value_type>;
        using difference_type = std::ptrdiff_t;
        using pointer = std::add_pointer_t<value_type>;
        using reference = std::add_lvalue_reference_t<value_type>;


        constexpr iterator () = default;
        constexpr iterator(const Value *ptr):_ptr(ptr) {};
        constexpr iterator (const iterator &other):_ptr(other._ptr) {}
        constexpr iterator &operator=(const iterator &other) {
            _ptr = other._ptr;
            return *this;
        }

        constexpr reference operator*() const {
            return load();
        }

        constexpr pointer operator->() const {
            return &load();
        }

        constexpr iterator &operator++() {_ptr+=2; return *this;}
        constexpr iterator &operator--() {_ptr-=2; return *this;}
        constexpr iterator operator++(int) {auto tmp = *this;_ptr+=2; return tmp;}
        constexpr iterator operator--(int) {auto tmp = *this;_ptr-=2; return tmp;}
        constexpr iterator &operator+=(difference_type n) {_ptr+=2*n; return *this;}
        constexpr iterator &operator-=(difference_type n) {_ptr-=2*n; return *this;}
        constexpr iterator operator+(difference_type n) const {return _ptr+2*n;}
        constexpr iterator operator-(difference_type n) const {return _ptr-2*n;}
        constexpr difference_type operator-(const iterator &other) const {return (_ptr-other._ptr)/2;}

        constexpr bool operator==(const iterator &other) const {return _ptr == other._ptr;}
        constexpr bool operator!=(const iterator &other) const {return _ptr != other._ptr;}
        constexpr bool operator>=(const iterator &other) const {return _ptr >= other._ptr;}
        constexpr bool operator<=(const iterator &other) const {return _ptr <= other._ptr;}
        constexpr bool operator>(const iterator &other) const {return _ptr > other._ptr;}
        constexpr bool operator<(const iterator &other) const {return _ptr < other._ptr;}

    protected:
        const Value *_ptr = nullptr;
        mutable const Value *_loaded = nullptr;
        mutable std::optional<value_type> _loaded_value = {};

        constexpr reference load() const {
            if (_loaded != _ptr) {
                _loaded = _ptr;
                _loaded_value.emplace(_ptr[0].template as<std::string_view>(), _ptr[1]);
            }
            return *_loaded_value;
        }
    };

    constexpr const Value *data() const {return _items;}


    constexpr iterator begin() const {return _items;}
    constexpr iterator end() const {return _items+_count;}


    constexpr bool empty() const {
        return _count == 0;
    }


protected:
    const Value *_items = nullptr;
    std::size_t _count = 0;
};



template<typename Iter>
class pair_iterator  {
public:
    using T = typename std::iterator_traits<Iter>::value_type;
    using iterator_category  = typename std::iterator_traits<Iter>::iterator_category;
    using difference_type = typename std::iterator_traits<Iter>::difference_type;

    struct solid  {
        T first = {};
        T second = {};
    };

    struct ref {
        T *first =  {};
        T *second = {};
    };

    class  value_type  {
    public:
        bool _is_ref ;
        union  {
            solid _solid;
            ref  _ref;
        };

        constexpr value_type():_is_ref(false),  _solid()  {}
        constexpr value_type(T &first, T &second):_is_ref(true), _ref{&first, &second} {}
        constexpr ~value_type() {
            if  (!_is_ref) {
                _solid.~solid();
            }
        }
        constexpr value_type(const value_type &other):_is_ref(false),_solid{other.first(), other.second()} {}
        constexpr value_type &operator=(const value_type &other) {
            first() = other.first();
            second() = other.second();
            return *this;
        }
        constexpr value_type(value_type &&other):_is_ref(false),_solid{std::move(other.first()), std::move(other.second())} {}
        constexpr value_type &operator=(value_type &&other) {
            first() = std::move(other.first());
            second() = std::move(other.second());
            return *this;
        }

        constexpr T &first() {return _is_ref?*_ref.first:_solid.first;}
        constexpr T &second() {return _is_ref?*_ref.second:_solid.second;}
        constexpr const T &first() const  {return _is_ref?*_ref.first:_solid.first;}
        constexpr const T &second() const {return _is_ref?*_ref.second:_solid.second;}
    };

    constexpr pair_iterator() = default;
    constexpr pair_iterator(Iter x):_first(x),_second(x) {}
    constexpr pair_iterator(const pair_iterator &other):_first(other._first) {}
    constexpr pair_iterator(pair_iterator &other):_first(std::move(other._first)) {}
    constexpr pair_iterator &operator=(const pair_iterator &other) {
        _first = other._first;
        return *this;
    }
    constexpr pair_iterator &operator=( pair_iterator &&other) {
        _first = std::move(other._first);
        return *this;
    }
    constexpr bool operator==(const pair_iterator &other) const {
        return _first == other._first;
    }
    constexpr bool operator<(const pair_iterator &other) const {
        return _first< other._first;
    }
    constexpr bool operator>(const pair_iterator &other) const {
        return _first> other._first;
    }
    constexpr bool operator<=(const pair_iterator &other) const {
        return _first<= other._first;
    }
    constexpr bool operator>=(const pair_iterator &other) const {
        return _first>= other._first;
    }
    constexpr difference_type operator-(const pair_iterator &other) const {
        return (_first - other._first)/2;
    }
    constexpr pair_iterator operator+(difference_type n) const {
        return _first+2*n;
    }
    constexpr pair_iterator operator-(difference_type n) const {
        return _first-2*n;
    }
    constexpr pair_iterator &operator+=(difference_type n) {
         _first+=2*n; return *this;
    }
    constexpr pair_iterator &operator-=(difference_type n) {
         _first-=2*n; return *this;
    }
    constexpr pair_iterator &operator++()  {
         _first+=2; return *this;
    }
    constexpr pair_iterator &operator--()  {
         _first-=2; return *this;
    }
    constexpr pair_iterator operator++(int)  {
         auto tmp = *this;
         _first+=2;
         return tmp;
    }
    constexpr pair_iterator operator--(int)  {
         auto tmp = *this;
         _first-=2;
         return tmp;
    }
    constexpr value_type &operator*()  const {
        _second = _first;
        std::advance(_second,1);
        _tmp.emplace(*_first, *_second);
        return *_tmp;
    }
    constexpr value_type *operator->() const {
        return &operator *();
    }


protected:
    Iter  _first = {};
    mutable Iter  _second = {};
    mutable std::optional<value_type> _tmp = {};
};


}
