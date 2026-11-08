#pragma once

#include "compat/linkage-macros.hpp"

#ifdef BUILD_HEAD_TEST
#   define OTR_HEAD_TEST_EXPORT OTR_GENERIC_EXPORT
#else
#   define OTR_HEAD_TEST_EXPORT OTR_GENERIC_IMPORT
#endif
