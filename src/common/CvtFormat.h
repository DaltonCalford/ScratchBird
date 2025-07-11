#ifndef COMMON_CVT_FORMAT_H
#define COMMON_CVT_FORMAT_H

#include "firebird.h"
#include "../common/cvt.h"

ScratchBird::string CVT_format_datetime_to_string(const dsc* desc, const ScratchBird::string& format, ScratchBird::Callbacks* cb);
ISC_TIMESTAMP_TZ CVT_format_string_to_datetime(const dsc* desc, const ScratchBird::string& format,
	const ScratchBird::EXPECT_DATETIME expectedType, ScratchBird::Callbacks* cb);

#endif // COMMON_CVT_FORMAT_H
