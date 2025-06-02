#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            Swap(other);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    Vector(const Vector& other) {
        if (other.size_ > 0) {
            RawMemory<T> new_data(other.size_);
            std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, new_data.GetAddress());
            data_ = std::move(new_data);
            size_ = other.size_;
        }
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    explicit Vector(size_t size) {
        if (size > 0) {
            RawMemory<T> new_data(size);
            std::uninitialized_value_construct_n(new_data.GetAddress(), size);
            data_ = std::move(new_data);
            size_ = size;
        }
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ == 0) {
                Clear();
            } else if (data_.Capacity() >= rhs.size_) {
                size_t common_size = std::min(size_, rhs.size_);
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + common_size, data_.GetAddress());
                if (rhs.size_ > size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                              rhs.size_ - size_,
                                              data_.GetAddress() + size_);
                } else if (rhs.size_ < size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                size_ = rhs.size_;
            } else {
                RawMemory<T> new_data(rhs.size_);
                std::uninitialized_copy_n(rhs.data_.GetAddress(), rhs.size_, new_data.GetAddress());
                Clear();
                data_ = std::move(new_data);
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if (size_ > 0) {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_ = std::move(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        } else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            const size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            new (new_data + size_) T(std::forward<Args>(args)...);

            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
            } catch (...) {
                std::destroy_at(new_data + size_);
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }

        ++size_;
        return data_[size_ - 1];
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept { return data_.GetAddress(); }
    iterator end() noexcept { return data_.GetAddress() + size_; }
    const_iterator begin() const noexcept { return data_.GetAddress(); }
    const_iterator end() const noexcept { return data_.GetAddress() + size_; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        iterator result = nullptr;
        size_t shift = pos - begin();
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new (new_data + shift) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), shift, new_data.GetAddress());
                std::uninitialized_move_n(begin() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
            } else {
                try {
                    std::uninitialized_copy_n(begin(), shift, new_data.GetAddress());
                    std::uninitialized_copy_n(begin() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress() + shift, 1);
                    throw;
                }
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        } else {
            if (size_ != 0) {
                T temp(std::forward<Args>(args)...);
                new (data_ + size_) T(std::move(*(end() - 1)));
                try {
                    std::move_backward(begin() + shift, end() - 1, end());
                } catch (...) {
                    std::destroy_at(data_ + size_);
                    throw;
                }
                data_[shift] = std::move(temp);
                result = data_ + shift;
            } else {
                result = new (data_ + shift) T(std::forward<Args>(args)...);
            }
        }
        ++size_;
        return result;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= begin() && pos < end());
        size_t shift = pos - begin();
        std::move(begin() + shift + 1, end(), begin() + shift);
        PopBack();
        return begin() + shift;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    void Clear() noexcept {
        std::destroy_n(data_.GetAddress(), size_);
        size_ = 0;
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
