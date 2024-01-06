#pragma once

#include <atomic>


namespace json20 {

template<typename T>
union constexpr_counter {
    std::atomic<T> _cntr;
    T _ccntr;

    constexpr constexpr_counter(T val) {
        if (std::is_constant_evaluated()) {
            std::construct_at(&_ccntr, val);
        } else {
            std::construct_at(&_cntr, val);
        }
    }

    constexpr ~constexpr_counter() {
        if (std::is_constant_evaluated()) {
            std::destroy_at(&_ccntr);
        } else {
            std::destroy_at(&_cntr);
        }
    }

    constexpr T operator++() {
        if (std::is_constant_evaluated()) {
            return ++_ccntr;
        } else {
            return ++_cntr;
        }
    }

    constexpr T operator--() {
        if (std::is_constant_evaluated()) {
            return --_ccntr;
        } else {
            return --_cntr;
        }
    }

};


template<typename T>
class shared_array_t {
public:
    constexpr T *begin() {return _data;}
    constexpr T *end() {return begin() + _size;}
    constexpr const T *begin() const {return _data;}
    constexpr const T *end() const {return begin() + _size;}
    constexpr void add_ref() {
            ++_ref_cnt;
    }
    constexpr void release_ref() {
            if (--_ref_cnt == 0) {
                if (std::is_constant_evaluated()) {
                    delete this;
                } else {
                    std::destroy_at(this);
                    ::operator delete(this);
                }
            }
    }

    template<std::invocable<T *, T *> Fn>
    static constexpr shared_array_t *create(std::size_t size, Fn &&constructor) {
        if (std::is_constant_evaluated()) {
            T *data =  new T[size];
            return new shared_array_t(data, size, std::forward<Fn>(constructor));
        } else {
            void *ptr = ::operator new(sizeof(shared_array_t)+sizeof(T)*size);
            shared_array_t *obj = reinterpret_cast<shared_array_t *>(ptr);
            T *data = reinterpret_cast<T *>(obj+1);
            for (std::size_t i = 0; i < size; ++i) std::construct_at(data+i);
            new(obj) shared_array_t(data, size, std::forward<Fn>(constructor));
            return obj;
        }
    }
    constexpr ~shared_array_t() {
        if (std::is_constant_evaluated()) {
            delete [] _data;
        } else {
            for (std::size_t i = 0; i < _size; ++i) std::destroy_at(_data+i);
        }
    }

    constexpr std::size_t size() const {return _size;}

    template<std::invocable<T *, T *> Fn>
    constexpr shared_array_t(T *data, std::size_t size, Fn &&constructor):_data(data), _size(size) {
        constructor(begin(), end());
    }

protected:
    T *_data;
    constexpr_counter<long> _ref_cnt = {1};
    std::size_t  _size;




};


}
