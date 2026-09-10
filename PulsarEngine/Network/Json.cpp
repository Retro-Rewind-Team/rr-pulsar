#include <Network/Json.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace Network {
namespace Json {

static const char *SkipWhitespace(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

static const char *FindValueEnd(const char *p, const char *end) {
    if (p == nullptr || end == nullptr) return nullptr;
    p = SkipWhitespace(p, end);
    if (p >= end) return nullptr;

    if (*p == '"') {
        for (const char *cur = p + 1; cur < end; ++cur) {
            if (*cur == '\\') {
                if (++cur >= end) return nullptr;
            } else if (*cur == '"') {
                return cur + 1;
            }
        }
        return nullptr;
    }

    if (*p == '{' || *p == '[') {
        const char open = *p;
        const char close = open == '{' ? '}' : ']';
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (const char *cur = p; cur < end; ++cur) {
            const char c = *cur;
            if (inString) {
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    inString = false;
                continue;
            }
            if (c == '"')
                inString = true;
            else if (c == open)
                ++depth;
            else if (c == close && --depth == 0)
                return cur + 1;
        }
        return nullptr;
    }

    const char *cur = p;
    while (cur < end && *cur != ',' && *cur != ']' && *cur != '}') ++cur;
    while (cur > p && (cur[-1] == ' ' || cur[-1] == '\n' || cur[-1] == '\r' || cur[-1] == '\t')) --cur;
    if (cur <= p) return nullptr;
    return cur;
}

bool Parse(const char *json, u32 length, Value &out) {
    if (json == nullptr || length == 0) return false;
    const char *end = json + length;
    const char *start = SkipWhitespace(json, end);
    const char *valueEnd = FindValueEnd(start, end);
    if (valueEnd == nullptr) return false;
    out.start = start;
    out.end = valueEnd;
    return true;
}

bool Parse(const char *json, Value &out) {
    return json != nullptr && Parse(json, static_cast<u32>(strlen(json)), out);
}

bool FindArray(const char *json, Value &out) {
    if (json == nullptr) return false;
    const char *end = json + strlen(json);
    bool inString = false;
    bool escaped = false;
    for (const char *p = json; p < end; ++p) {
        if (inString) {
            if (escaped)
                escaped = false;
            else if (*p == '\\')
                escaped = true;
            else if (*p == '"')
                inString = false;
            continue;
        }
        if (*p == '"')
            inString = true;
        else if (*p == '[') {
            const char *arrayEnd = FindValueEnd(p, end);
            if (arrayEnd == nullptr) return false;
            out.start = p;
            out.end = arrayEnd;
            return true;
        }
    }
    return false;
}

static bool KeyEquals(const char *start, const char *end, const char *key) {
    if (key == nullptr) return false;
    const u32 keyLen = static_cast<u32>(strlen(key));
    return end - start == static_cast<s32>(keyLen) && strncmp(start, key, keyLen) == 0;
}

bool Find(const Value &object, const char *key, Value &out) {
    if (object.start == nullptr || object.end == nullptr || object.start >= object.end || *object.start != '{') return false;

    const char *p = object.start + 1;
    const char *end = object.end - 1;
    bool first = true;
    while (p < end) {
        p = SkipWhitespace(p, end);
        if (!first) {
            if (p >= end || *p != ',') return false;
            p = SkipWhitespace(p + 1, end);
        }
        if (p >= end || *p != '"') return false;

        const char *keyStart = p + 1;
        const char *keyEnd = FindValueEnd(p, end);
        if (keyEnd == nullptr) return false;
        const char *keyClose = keyEnd - 1;

        p = SkipWhitespace(keyEnd, end);
        if (p >= end || *p != ':') return false;
        p = SkipWhitespace(p + 1, end);

        const char *valueEnd = FindValueEnd(p, end);
        if (valueEnd == nullptr) return false;
        if (KeyEquals(keyStart, keyClose, key)) {
            out.start = p;
            out.end = valueEnd;
            return true;
        }
        p = valueEnd;
        first = false;
    }
    return false;
}

bool Next(const Value &array, const char *&cursor, Value &out) {
    if (array.start == nullptr || array.end == nullptr || array.start >= array.end || *array.start != '[') return false;
    const char *end = array.end - 1;
    const bool first = cursor == nullptr;
    const char *p = first ? array.start + 1 : cursor;
    p = SkipWhitespace(p, end);
    if (!first) {
        if (p >= end || *p != ',') return false;
        p = SkipWhitespace(p + 1, end);
    }
    if (p >= end) return false;

    const char *valueEnd = FindValueEnd(p, end);
    if (valueEnd == nullptr) return false;
    out.start = p;
    out.end = valueEnd;
    cursor = valueEnd;
    return true;
}

static bool GetUnsigned(const Value &value, u64 max, u64 &out) {
    if (value.start == nullptr || value.end == nullptr) return false;
    const char *p = SkipWhitespace(value.start, value.end);
    if (p >= value.end || *p < '0' || *p > '9') return false;

    out = 0;
    while (p < value.end && *p >= '0' && *p <= '9') {
        const u64 digit = static_cast<u64>(*p - '0');
        if (out > (max - digit) / 10) return false;
        out = out * 10 + digit;
        ++p;
    }
    return SkipWhitespace(p, value.end) == value.end;
}

bool GetU32(const Value &value, u32 &out) {
    u64 parsed = 0;
    if (!GetUnsigned(value, 0xffffffffu, parsed)) return false;
    out = static_cast<u32>(parsed);
    return true;
}

bool GetU64(const Value &value, u64 &out) {
    return GetUnsigned(value, ~static_cast<u64>(0), out);
}

bool GetS32(const Value &value, s32 &out) {
    if (value.start == nullptr || value.end == nullptr) return false;
    const char *p = SkipWhitespace(value.start, value.end);
    bool negative = false;
    if (p < value.end && *p == '-') {
        negative = true;
        ++p;
    }
    if (p >= value.end || *p < '0' || *p > '9') return false;

    u64 parsed = 0;
    const u64 max = negative ? 0x80000000u : 0x7fffffffu;
    while (p < value.end && *p >= '0' && *p <= '9') {
        const u64 digit = static_cast<u64>(*p - '0');
        if (parsed > (max - digit) / 10) return false;
        parsed = parsed * 10 + digit;
        ++p;
    }
    if (SkipWhitespace(p, value.end) != value.end) return false;
    const s64 signedValue = negative ? -static_cast<s64>(parsed) : static_cast<s64>(parsed);
    out = static_cast<s32>(signedValue);
    return true;
}

static unsigned char ParseEscape(const char *&p, const char *end) {
    if (p >= end) return '?';
    const unsigned char escaped = static_cast<unsigned char>(*p++);
    switch (escaped) {
        case '"':
        case '\\':
        case '/':
            return escaped;
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'u':
            for (int i = 0; i < 4 && p < end; ++i) ++p;
            return '?';
        default:
            return '?';
    }
}

template <typename T>
static bool GetStringImpl(const Value &value, T *out, u32 outLen, bool wide) {
    if (out == nullptr || outLen == 0) return false;
    out[0] = 0;
    if (value.start == nullptr || value.end == nullptr || value.start >= value.end || *value.start != '"') return false;

    const char *p = value.start + 1;
    const char *end = value.end - 1;
    u32 written = 0;
    while (p < end) {
        unsigned char c = static_cast<unsigned char>(*p++);
        if (c == '\\') c = ParseEscape(p, end);
        if (written + 1 < outLen) out[written++] = static_cast<T>(wide && c >= 0x80 ? '?' : c);
    }
    out[written] = 0;
    return true;
}

bool GetString(const Value &value, char *out, u32 outLen) { return GetStringImpl(value, out, outLen, false); }
bool GetString(const Value &value, wchar_t *out, u32 outLen) { return GetStringImpl(value, out, outLen, true); }

bool Get(const Value &object, const char *key, u32 &out) {
    Value value;
    return Find(object, key, value) && GetU32(value, out);
}
bool Get(const Value &object, const char *key, u64 &out) {
    Value value;
    return Find(object, key, value) && GetU64(value, out);
}
bool Get(const Value &object, const char *key, s32 &out) {
    Value value;
    return Find(object, key, value) && GetS32(value, out);
}
bool Get(const Value &object, const char *key, char *out, u32 outLen) {
    Value value;
    return Find(object, key, value) && GetString(value, out, outLen);
}
bool Get(const Value &object, const char *key, wchar_t *out, u32 outLen) {
    Value value;
    return Find(object, key, value) && GetString(value, out, outLen);
}

}  // namespace Json
}  // namespace Network
}  // namespace Pulsar
