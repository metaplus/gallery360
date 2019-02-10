#pragma once

namespace core
{
    struct coordinate
    {
        int col = 0;
        int row = 0;

        bool operator<(const coordinate& that) const;
        bool operator==(const coordinate& that) const;
    };

    size_t hash_value(const coordinate& coordinate);

    struct dimension
    {
        int width = 0;
        int height = 0;

        bool operator<(const dimension& that) const;
        bool operator==(const dimension& that) const;
    };
}
