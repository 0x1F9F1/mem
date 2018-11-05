/*
    Copyright 2018 Brick

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
    and associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if !defined(MEM_FAST_VECTOR_BRICK_H)
#define MEM_FAST_VECTOR_BRICK_H

#include "defines.h"

#include <cstring>
#include <cstdlib>

#include <type_traits>

namespace mem
{
    template <typename T>
    class fast_vector
    {
    private:
        static_assert(std::is_trivial<T>::value, "Type is not trivially copyable");

        T* data_ {nullptr};
        size_t size_ {0};
        size_t capacity_ {0};

        size_t calculate_growth(size_t new_size) const noexcept;
        void realloc_data(size_t length);

    public:
        fast_vector() noexcept = default;

        fast_vector(const fast_vector& other);
        fast_vector(fast_vector&& other) noexcept;

        ~fast_vector();

        fast_vector& operator=(const fast_vector& other);
        fast_vector& operator=(fast_vector&& other);

        void swap(fast_vector& other) noexcept;

        void reserve(size_t length);
        void resize(size_t length);

        void assign(const T* source, size_t length);
        void append(const T* source, size_t length);

        void push_back(const T& value);

        void clear();
        void shrink_to_fit();

        T& operator[](size_t index) noexcept;
        const T& operator[](size_t index) const noexcept;

        T* data() noexcept;
        T* begin() noexcept;
        T* end() noexcept;

        const T* data() const noexcept;
        const T* begin() const noexcept;
        const T* end() const noexcept;

        size_t size() const noexcept;
        size_t capacity() const noexcept;

        using value_type = T;

        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using reference = value_type&;
        using const_reference = const value_type&;

        using pointer = value_type*;
        using const_pointer = const value_type*;

        using iterator = value_type*;
        using const_iterator = const value_type*;
    };

    using byte_buffer = fast_vector<byte>;

    template <typename T>
    inline fast_vector<T>::fast_vector(const fast_vector<T>& other)
    {
        if (this != &other)
        {
            assign(other.data(), other.size());
        }
    }

    template <typename T>
    inline fast_vector<T>::fast_vector(fast_vector<T>&& other) noexcept
    {
        if (this != &other)
        {
            swap(other);
        }
    }

    template <typename T>
    inline fast_vector<T>::~fast_vector()
    {
        if (data_)
        {
            free(data_);
        }
    }

    template <typename T>
    inline fast_vector<T>& fast_vector<T>::operator=(const fast_vector<T>& other)
    {
        if (this != &other)
        {
            assign(other.data(), other.size());
        }

        return *this;
    }

    template <typename T>
    inline fast_vector<T>& fast_vector<T>::operator=(fast_vector<T>&& other)
    {
        if (this != &other)
        {
            clear();

            swap(other);
        }

        return *this;
    }

    template <typename T>
    inline size_t fast_vector<T>::calculate_growth(size_t new_size) const noexcept
    {
        size_t old_capacity = capacity();
        size_t new_capacity = old_capacity + (old_capacity >> 1);

        if (new_capacity < new_size)
        {
            new_capacity = new_size;
        }

        return new_capacity;
    }

    template <typename T>
    inline void fast_vector<T>::realloc_data(size_t length)
    {
        void* new_data = realloc(data_, length * sizeof(T));

        if (new_data)
        {
            data_ = static_cast<T*>(new_data);
        }
        else
        {
            abort();
        }
    }

    template <typename T>
    inline void fast_vector<T>::swap(fast_vector<T>& other) noexcept
    {
        if (this != &other)
        {
            T* temp_data = data_;
            size_t temp_size = size_;
            size_t temp_max_size = capacity_;

            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;

            other.data_ = temp_data;
            other.size_ = temp_size;
            other.capacity_ = temp_max_size;
        }
    }

    template <typename T>
    inline void fast_vector<T>::reserve(size_t length)
    {
        if (length > capacity_)
        {
            realloc_data(length);
            capacity_ = length;
        }
    }

    template <typename T>
    inline void fast_vector<T>::resize(size_t length)
    {
        realloc_data(length);
        size_ = length;
        capacity_ = length;
    }

    template <typename T>
    inline void fast_vector<T>::assign(const T* source, size_t length)
    {
        clear();

        append(source, length);
    }

    template <typename T>
    inline void fast_vector<T>::append(const T* source, size_t length)
    {
        size_t new_size = size_ + length;

        reserve(calculate_growth(new_size));
        memcpy(data_ + size_, source, length * sizeof(T));

        size_ = new_size;
    }

    template <typename T>
    inline void fast_vector<T>::push_back(const T& value)
    {
        append(&value, 1);
    }

    template <typename T>
    inline void fast_vector<T>::clear()
    {
        if (data_)
        {
            free(data_);
        }

        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    template <typename T>
    inline void fast_vector<T>::shrink_to_fit()
    {
        resize(size_);
    }

    template <typename T>
    inline T& fast_vector<T>::operator[](size_t index) noexcept
    {
        return data_[index];
    }

    template <typename T>
    inline const T& fast_vector<T>::operator[](size_t index) const noexcept
    {
        return data_[index];
    }

    template <typename T>
    inline T* fast_vector<T>::data() noexcept
    {
        return data_;
    }

    template <typename T>
    inline T* fast_vector<T>::begin() noexcept
    {
        return data_;
    }

    template <typename T>
    inline T* fast_vector<T>::end() noexcept
    {
        return data_ + size_;
    }

    template <typename T>
    inline const T* fast_vector<T>::data() const noexcept
    {
        return data_;
    }

    template <typename T>
    inline const T* fast_vector<T>::begin() const noexcept
    {
        return data_;
    }

    template <typename T>
    inline const T* fast_vector<T>::end() const noexcept
    {
        return data_ + size_;
    }

    template <typename T>
    inline size_t fast_vector<T>::size() const noexcept
    {
        return size_;
    }

    template <typename T>
    inline size_t fast_vector<T>::capacity() const noexcept
    {
        return capacity_;
    }
}

#endif // MEM_FAST_VECTOR_BRICK_H
