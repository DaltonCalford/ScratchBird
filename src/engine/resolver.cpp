#include "scratchbird/engine/resolver.h"

#include <string>
#include <string_view>

namespace scratchbird::engine
{
    std::string casefold_ascii(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s)
            out.push_back(static_cast<char>(std::tolower(c)));
        return out;
    }

    std::string casefold_unicode(const std::string& s)
    {
#if defined(SCRATCHBIRD_WITH_ICU)
        // ICU-based full Unicode case folding using NFKC_Casefold equivalent
        // Convert UTF-8 -> UChar, fold, then back to UTF-8
        UErrorCode status = U_ZERO_ERROR;
        // Convert to UChar
        int32_t srcLen = static_cast<int32_t>(s.size());
        int32_t uLen = 0;
        u_strFromUTF8(nullptr, 0, &uLen, s.c_str(), srcLen, &status);
        status = U_ZERO_ERROR;
        std::u16string ustr;
        ustr.resize(static_cast<size_t>(uLen));
        u_strFromUTF8(reinterpret_cast<UChar*>(ustr.data()), uLen, nullptr, s.c_str(), srcLen,
                      &status);
        // Case fold
        int32_t fLen = u_strFoldCase(nullptr, 0, reinterpret_cast<const UChar*>(ustr.data()), uLen,
                                     U_FOLD_CASE_DEFAULT, &status);
        status = U_ZERO_ERROR;
        std::u16string fstr;
        fstr.resize(static_cast<size_t>(fLen));
        u_strFoldCase(reinterpret_cast<UChar*>(fstr.data()), fLen,
                      reinterpret_cast<const UChar*>(ustr.data()), uLen, U_FOLD_CASE_DEFAULT,
                      &status);
        // Normalize to NFKC (optional)
        UBool isNorm = false;
        unorm2_isNormalized(unorm2_getNFKCInstance(&status),
                            reinterpret_cast<const UChar*>(fstr.data()), fLen, &status);
        // Convert back to UTF-8
        int32_t outLen = 0;
        u_strToUTF8(nullptr, 0, &outLen, reinterpret_cast<const UChar*>(fstr.data()), fLen,
                    &status);
        status = U_ZERO_ERROR;
        std::string out;
        out.resize(static_cast<size_t>(outLen));
        u_strToUTF8(out.data(), outLen, nullptr, reinterpret_cast<const UChar*>(fstr.data()), fLen,
                    &status);
        return out;
#elif defined(SCRATCHBIRD_WITH_UTF8PROC)
        // utf8proc NFKC_Casefold
        utf8proc_uint8_t* result =
            utf8proc_NFKC_Casefold(reinterpret_cast<const utf8proc_uint8_t*>(s.c_str()));
        if (!result)
            return casefold_ascii(s);
        std::string out(reinterpret_cast<char*>(result));
        free(result);
        return out;
#else
        return casefold_ascii(s);
#endif
    }
} // namespace scratchbird::engine
