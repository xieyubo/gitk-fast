module;

#include <algorithm>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <vector>

export module gitkf:ring_buffer;

export template <typename T, size_t N>
    requires(N > 0)
class ring_buffer;

template <typename T, size_t N>
class ring_buffer_iterator {
public:
    ring_buffer_iterator(ring_buffer<T, N>* pRing, size_t index) noexcept
        : m_pRing { pRing }
        , m_index { index }
    {
    }

    T* operator->() noexcept { return &(*m_pRing)[m_index]; }
    T& operator*() noexcept { return (*m_pRing)[m_index]; }

    ring_buffer_iterator& operator++() noexcept
    {
        m_index = ++m_index % N;
        return *this;
    }
    bool operator!=(const ring_buffer_iterator& other) noexcept { return m_index != other.m_index; }

private:
    ring_buffer<T, N>* m_pRing {};
    size_t m_index {};
};

export template <typename T, size_t N>
    requires(N > 0)
class ring_buffer {
public:
    using value_type = T;
    using iterator = ring_buffer_iterator<T, N>;

    ring_buffer() noexcept = default;
    ring_buffer(const ring_buffer&) = delete;

    iterator begin() noexcept { return iterator { this, m_head }; }
    iterator end() noexcept { return iterator { this, m_tail }; }

    value_type& operator[](size_t index) noexcept { return reinterpret_cast<value_type&>(m_data[index]); }

    void push_front(T&& value)
    {
        // Move head to previous position.
        m_head = m_head ? m_head - 1 : N - 1;

        if (m_head == m_tail) {
            // Full, delete the old one.
            m_tail = m_tail ? m_tail - 1 : N - 1;
            reinterpret_cast<T*>(&m_data[m_tail])->~T();
        }

        // save the new one.
        new (m_data + m_head) T { std::move(value) };
    }

private:
    std::aligned_storage_t<sizeof(T), alignof(T)> m_data[N + 1];
    size_t m_head {};
    size_t m_tail {};
};