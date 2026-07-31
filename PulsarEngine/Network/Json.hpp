#ifndef PULSAR_NETWORK_JSON_HPP
#define PULSAR_NETWORK_JSON_HPP

#include <types.hpp>

namespace Pulsar {
namespace Network {
namespace Json {

const char* SkipWhitespace(const char* p);
const char* SkipWhitespace(const char* p, const char* end);

const char* ParseU32(const char* p, u32& out);
const char* ParseU64(const char* p, u64& out);
bool ParseU32(const char*& p, const char* end, u32& out);

}  // namespace Json
}  // namespace Network
}  // namespace Pulsar

#endif  // PULSAR_NETWORK_JSON_HPP
