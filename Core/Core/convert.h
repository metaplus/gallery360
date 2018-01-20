#pragma once

namespace core
{
    template<typename Entity>
    class CORE_API convert //: Entity
    {
    public:
        using source = typename Entity::source;
        using sink = typename Entity::sink;
        convert() = default;
        template<typename ...Types>
        explicit convert(Types&& ...args);
        Entity& value();
        //template<typename Func>        //std::is_default_constructible_v
        boost::optional<sink> run_one(boost::optional<source&> source);//, Func&& predicate);
    private:
        std::shared_ptr<Entity> instance_;
    };

    template <typename Entity>
    template <typename ...Types>
    convert<Entity>::convert(Types&& ...args)
        : instance_{ std::make_shared<Entity>(std::forward<Types>(args)...) }
    {
    }

    template <typename Entity>
    Entity& convert<Entity>::value()
    {
        return *instance_;
    }

    template <typename Entity>
    boost::optional<typename convert<Entity>::sink> 
    convert<Entity>::run_one(boost::optional<source&> source)
    {
        static_assert(std::is_nothrow_move_constructible_v<sink>,"noexcept specified move ctor");
        return std::move(instance_->run_one(source));
    }
}
