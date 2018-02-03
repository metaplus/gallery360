// Monitor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
namespace filesystem = std::experimental::filesystem;



struct MyRecord
{
    uint8_t x, y;
    float z;

    template <class Archive>
    void serialize(Archive & ar)
    {
        ar(x, y, z);
    }
};

struct SomeData
{
    int32_t id;
    std::shared_ptr<std::unordered_map<uint32_t, MyRecord>> data;

    template <class Archive>
    void save(Archive & ar) const
    {
        ar(data);
    }

    template <class Archive>
    void load(Archive & ar)
    {
        static int32_t idGen = 0;
        id = idGen++;
        ar(data);
    }
};

int main()
{
    std::stringstream os;
    {
        cereal::BinaryOutputArchive archive(os);
        std::variant<int, double> var = 1.2;
        SomeData myData;
        archive(var);
    }
    {   
        os.str(""s);
        os.clear();
        cereal::BinaryOutputArchive archive(os);
        std::variant<int, double> var = 200;
        archive(var);
        auto vxyz = var.index();
    }
    {
        cereal::BinaryInputArchive archive(os);
        std::variant<int, double> var;
        archive(var);
        auto vxyz = var.index();
    }
    return 0;
}