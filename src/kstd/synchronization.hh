#pragma once

#include "basic.hh"

#if OS == GAMEOS
#include "gameos/synchronization.hh"
#elif OS == LINUX
#include "linux/synchronization.hh"
#else
#error "Unsupported OS"
#endif
