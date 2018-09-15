#include "stdafx.h"
#include "component.h"

namespace media::component
{
    struct frame_segmentor::impl
    {
        int64_t consume_count = 0;
        multi_buffer init_buffer;
        multi_buffer current_buffer;
    };

    bool frame_segmentor::context_valid() const noexcept {
        return impl_ != nullptr;
    }
}
