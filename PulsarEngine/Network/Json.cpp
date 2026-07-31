#include <Network/Json.hpp>

namespace Pulsar {
namespace Network {
namespace Json {

const char* SkipWhitespace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

const char* SkipWhitespace(const char* p, const char* end) {
    if (p == nullptr || end == nullptr) return p;
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

template <typename T>
static const char* ParseUnsigned(const char* p, T& out) {
    out = 0;
    p = SkipWhitespace(p);
    if (p == nullptr || *p == '-' || *p < '0' || *p > '9') return nullptr;
    while (*p >= '0' && *p <= '9') {
        out = out * 10 + static_cast<T>(*p - '0');
        ++p;
    }
    return p;
}

const char* ParseU32(const char* p, u32& out) {
    return ParseUnsigned(p, out);
}

const char* ParseU64(const char* p, u64& out) {
    return ParseUnsigned(p, out);
}

bool ParseU32(const char*& p, const char* end, u32& value) {
    if (p == nullptr || end == nullptr) return false;
    p = SkipWhitespace(p, end);
    if (p >= end || *p < '0' || *p > '9') return false;

    value = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        const u32 digit = static_cast<u32>(*p - '0');
        if (value > (0xffffffffu - digit) / 10) return false;
        value = value * 10 + digit;
        ++p;
    }
    return true;
}

}  // namespace Json
}  // namespace Network
}  // namespace Pulsar
