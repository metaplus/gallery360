#pragma once

//raw pointer wrapper  
namespace core
{
    inline namespace v1
    {
        template <typename T>
        class basic_ptr
        {
        public:
            using value_type = T;
            using reference = T&;
            using const_reference = const T&;
            using pointer_type = T*;
            using pointer_reference = T*&;
            using pointer_const_reference = T* const&;
            using pointer_pointer_type = T**;
		    using const_pointer_type = T* const;
            basic_ptr(const_pointer_type init = nullptr) :ptr_{ init } {}
            //basic_ptr(const basic_ptr& other);
            bool reset(const_pointer_type other = nullptr);
		    bool empty() const noexcept { return ptr_ == nullptr; }
            reference operator*() { return *ptr_; }                
            const_reference operator*() const { return *ptr_; }    
            pointer_reference operator->() noexcept { return ptr_; }
		    pointer_const_reference operator->() const noexcept { return ptr_; }
            //pointer_type* operator&() { return std::addressof(ptr_); }                //hazardous
            explicit operator pointer_reference() noexcept { return ptr_; }              
            explicit operator pointer_const_reference() const noexcept { return ptr_; }
        protected:
            pointer_type ptr_;
        };
        template <typename T>
        bool basic_ptr<T>::reset(const_pointer_type other)
        {
            if (other) ptr_ = other;
            return other != nullptr;
        }
    }
    namespace v2
    {
        template <typename T>
        class basic_ptr
        {
        public:
            using value_type = T;
            using reference = T&;
            using const_reference = const T&;
            using pointer_type = T*;
            using pointer_reference = T*&;
            using pointer_const_reference = T* const&;
            using pointer_pointer_type = T**;
		    using const_pointer_type = T* const;
            basic_ptr(const_pointer_type init = nullptr) :ptr_{ init } {}
            //basic_ptr(const basic_ptr& other);
            void reset(const_pointer_type other = nullptr);
            template<typename Deleter>
            void reset(const_pointer_type other, Deleter del);
		    bool empty() const noexcept { return ptr_==nullptr; }
            reference operator*() const { return *ptr_; }                
		    pointer_type operator->() const noexcept { return ptr_.get(); }
        protected:
            std::shared_ptr<value_type> ptr_;
        };
        template <typename T>
        void basic_ptr<T>::reset(const_pointer_type other)
        {
            if(ptr_.use_count()>1)
                throw std::runtime_error{"prohibit access contention"};  
            auto deleter=std::get_deleter<void(*)(T*)>(ptr_);
            deleter!=nullptr?
                ptr_.reset(other,*deleter):
                ptr_.reset(other);
        }
        template <typename T>
        template <typename Deleter>
        void basic_ptr<T>::reset(const_pointer_type other, Deleter del)
        {
            if(ptr_.use_count()>1)
                throw std::runtime_error{"prohibit access contention"};
            ptr_.reset(other,del);
        }
    }

}




