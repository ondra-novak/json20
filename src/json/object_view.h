#pragma once

#include "common.h"

namespace JSON20_NAMESPACE_NAME {



template<typename Value>
class object_view_gen {
public:



    constexpr object_view_gen() = default;
    constexpr object_view_gen(const Value *items, std::size_t count):_items(items), _count(count) {}
    constexpr object_view_gen(const Value *beg, const Value *end):_items(beg), _count(end - beg) {}

    constexpr std::size_t size() const {return _count/2;}

    constexpr const Value &operator[](std::size_t pos) const {
        if (pos *2 >= _count) return get_undefined();
        return _items[pos*2+1];
    }

    constexpr std::string_view key_at(std::size_t pos) const {
        if (pos *2 >= _count) return {};
        return _items[pos*2].template as<std::string_view>();
    }

    static constexpr const Value &get_undefined();

    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = const Value;
        using difference_type = std::ptrdiff_t;
        using reference = std::add_lvalue_reference_t<value_type>;
        using pointer = std::add_pointer_t<value_type>;

        constexpr iterator () = default;
        constexpr iterator(const Value *ptr):_ptr(ptr) {};

        constexpr reference operator*() const {
            return _ptr[1];
        }

        constexpr pointer operator->() const {
            return _ptr+1;
        }
        constexpr std::string_view key() const {
            return _ptr->template as<std::string_view>();
        }

        constexpr const Value &key_value() const {
            return _ptr[0];
        }
        constexpr const Value &value() const {
            return _ptr[1];
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
    };

    constexpr const Value *data() const {return _items;}


    constexpr iterator begin() const {return _items;}
    constexpr iterator end() const {return _items+_count;}

    ///convert iterator from value to key iterator
    /**
     * @param iter iterator which iterates values
     * @return iterator which iterates keys
     */
    static constexpr iterator value2key(iterator iter) {
        std::advance(iter,-1);
        return iterator(&(*iter));
    }
    ///convert iterator from key to value iterator
    /**
     * @param iter iterator which iterates values
     * @return iterator which iterates keys
     *
     * @note iterator points to corresponding value
     */
    static constexpr iterator key2value(iterator iter) {
        return iterator(&(*iter));
    }

    constexpr bool empty() const {
        return _count == 0;
    }


protected:
    const Value *_items = nullptr;
    std::size_t _count = 0;
};


template<typename T>
class virtual_pair {
public:
    T &first;
    T &second;

    constexpr virtual_pair(T &a, T &b):first(a), second(b) {}
    constexpr virtual_pair(const virtual_pair &other):first(other.first), second(other.second) {}
    constexpr virtual_pair &operator=(const virtual_pair &other) {
        first = other.first;
        second = other.second;
        return *this;
    }
    constexpr void swap(virtual_pair &other) {
        std::swap(first, other.first);
        std::swap(second, other.second);
    }
};

template<typename T>
struct virtual_ref {
    T *ptr;
    constexpr operator std::pair<T,T>() const {return {ptr[0], ptr[1]};}
    constexpr operator std::pair<T &,T &>() const {return {ptr[0], ptr[1]};}
    constexpr virtual_ref &operator=(const std::pair<T,T> &vt) {
        ptr[0] = vt.first;
        ptr[1] = vt.second;
        return *this;
    }
    void swap(const virtual_ref<T> &other) const {
        std::swap(ptr[0], other.ptr[0]);
        std::swap(ptr[1], other.ptr[1]);
    }
};

template<typename T>
constexpr void swap(JSON20_NAMESPACE_NAME::virtual_ref<T> a, JSON20_NAMESPACE_NAME::virtual_ref<T> b) {
    a.swap(b);
}

template<typename T>
class pair_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::pair<T, T>;
    using difference_type = std::ptrdiff_t;


    using reference = virtual_ref<T>;

    constexpr pair_iterator () = default;
    constexpr pair_iterator(T *ptr):_ptr(ptr) {};

    constexpr reference operator*() const {
        return {_ptr};
    }


    constexpr pair_iterator &operator++() {_ptr+=2; return *this;}
    constexpr pair_iterator &operator--() {_ptr-=2; return *this;}
    constexpr pair_iterator operator++(int) {auto tmp = *this;_ptr+=2; return tmp;}
    constexpr pair_iterator operator--(int) {auto tmp = *this;_ptr-=2; return tmp;}
    constexpr pair_iterator &operator+=(difference_type n) {_ptr+=2*n; return *this;}
    constexpr pair_iterator &operator-=(difference_type n) {_ptr-=2*n; return *this;}
    constexpr pair_iterator operator+(difference_type n) const {return _ptr+2*n;}
    constexpr pair_iterator operator-(difference_type n) const {return _ptr-2*n;}
    constexpr difference_type operator-(const pair_iterator &other) const {return (_ptr-other._ptr)/2;}

    constexpr bool operator==(const pair_iterator &other) const {return _ptr == other._ptr;}
    constexpr bool operator!=(const pair_iterator &other) const {return _ptr != other._ptr;}
    constexpr bool operator>=(const pair_iterator &other) const {return _ptr >= other._ptr;}
    constexpr bool operator<=(const pair_iterator &other) const {return _ptr <= other._ptr;}
    constexpr bool operator>(const pair_iterator &other) const {return _ptr > other._ptr;}
    constexpr bool operator<(const pair_iterator &other) const {return _ptr < other._ptr;}


protected:
    T *_ptr = nullptr;

};

}
