#include "stdafx.h"
#include "interprocess.h"

void ipc::channel::clear()
{
    terminate_.set_value();
}
