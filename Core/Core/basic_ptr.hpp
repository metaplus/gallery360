#pragma once

namespace core
{
    inline namespace v1
    {   
        template <typename T>
        class basic_ptr     //raw pointer wrapper  
        {
        public:
            using value_type = T;
            using reference = T & ;
            using const_reference = const T&;
            using pointer = T* ;
            using const_pointer = T * const;
            using pointer_reference = T* &;
            using pointer_const_reference = T* const&;
            using pointer_pointer = T**;
            basic_ptr(const_pointer init = nullptr) :ptr_{ init } {}
            //basic_ptr(const basic_ptr& other);
            void reset(const_pointer other = nullptr) { ptr_ = other; }
            bool empty() const noexcept { return ptr_ == nullptr; }
            reference operator*() { return *ptr_; }
            const_reference operator*() const { return *ptr_; }
            pointer_reference operator->() noexcept { return ptr_; }
            pointer_const_reference operator->() const noexcept { return ptr_; }
            //pointer* operator&() { return std::addressof(ptr_); }    //hazardous potientially
            explicit operator pointer_reference() noexcept { return ptr_; }
            explicit operator pointer_const_reference() const noexcept { return ptr_; }
        protected:
            pointer ptr_;
        };
    }
    namespace v2
    {
        template <typename T>
        class basic_ptr     //smart pointer based
        {
        public:
            using value_type = T;
            using reference = T & ;
            using const_reference = const T&;
            using pointer = T* ;
            using pointer_reference = T* &;
            using pointer_const_reference = T* const&;
            using pointer_pointer = T**;
            using const_pointer = T* const;
            basic_ptr(const_pointer init = nullptr) :ptr_{ init } {}
            //basic_ptr(const basic_ptr& other);
            void reset(const_pointer other = nullptr);
            template<typename Deleter>
            void reset(const_pointer other, Deleter del);
            bool empty() const noexcept { return ptr_ == nullptr; }
            reference operator*() const { return *ptr_; }
            pointer operator->() const noexcept { return ptr_.get(); }
        protected:
            std::shared_ptr<value_type> ptr_;
        };
        template <typename T>
        void basic_ptr<T>::reset(const_pointer other)
        {
            if (ptr_.use_count() > 1)
                throw std::runtime_error{ "prohibit access contention" };
            auto deleter = std::get_deleter<void(*)(T*)>(ptr_);
            deleter != nullptr ? ptr_.reset(other, *deleter) : ptr_.reset(other);
        }
        template <typename T>
        template <typename Deleter>
        void basic_ptr<T>::reset(const_pointer other, Deleter del)
        {
            if (ptr_.use_count() > 1)
                throw std::runtime_error{ "prohibit access contention" };
            ptr_.reset(other, del);
        }
    }
}




