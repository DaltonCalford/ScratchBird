#ifndef SCRATCHBIRD_ENGINE_RESOLVER_H
#define SCRATCHBIRD_ENGINE_RESOLVER_H

#include <string>

namespace scratchbird::engine
{
    // ASCII-only lowercasing used as fallback
    std::string casefold_ascii(const std::string& s);

    // Unicode casefold using ICU/utf8proc when available; falls back to ASCII
    std::string casefold_unicode(const std::string& s);
} // namespace scratchbird::engine

#endif
