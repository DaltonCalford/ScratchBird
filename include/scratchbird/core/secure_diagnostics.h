#pragma once

#include <string>
#include <string_view>

namespace scratchbird::core {

bool isSensitiveDiagnosticFieldName(std::string_view key);

std::string redactSensitiveDiagnosticText(const std::string& input);

std::string redactSensitiveDiagnosticField(std::string_view key,
                                          const std::string& value);

}  // namespace scratchbird::core
