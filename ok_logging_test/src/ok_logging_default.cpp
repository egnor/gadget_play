#include "ok_logging.h"

// Default logging config, in a separate file to support weak linkage.
extern char const* const ok_logging_config __attribute__((weak)) = nullptr;
