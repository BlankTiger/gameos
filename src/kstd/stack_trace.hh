#pragma once

#include "basic.hh"

#if OS == GAMEOS
#include "gameos/stack_trace.hh"
#elif OS == LINUX
#include "linux/stack_trace.hh"
#else
#error "Unsupported OS"
#endif
