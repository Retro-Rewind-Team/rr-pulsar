#ifndef PULSAR_NETWORK_JSON_HPP
#define PULSAR_NETWORK_JSON_HPP

#include <types.hpp>

namespace Pulsar {
namespace Network {
namespace Json {

struct Value {
    const char *start;
    const char *end;
};

bool Parse(const char *json, Value &out);
bool Parse(const char *json, u32 length, Value &out);
bool FindArray(const char *json, Value &out);
bool Find(const Value &object, const char *key, Value &out);
bool Next(const Value &array, const char *&cursor, Value &out);

bool GetU32(const Value &value, u32 &out);
bool GetU64(const Value &value, u64 &out);
bool GetS32(const Value &value, s32 &out);
bool GetString(const Value &value, char *out, u32 outLen);
bool GetString(const Value &value, wchar_t *out, u32 outLen);

bool Get(const Value &object, const char *key, u32 &out);
bool Get(const Value &object, const char *key, u64 &out);
bool Get(const Value &object, const char *key, s32 &out);
bool Get(const Value &object, const char *key, char *out, u32 outLen);
bool Get(const Value &object, const char *key, wchar_t *out, u32 outLen);

}  // namespace Json
}  // namespace Network
}  // namespace Pulsar

#endif  // PULSAR_NETWORK_JSON_HPP