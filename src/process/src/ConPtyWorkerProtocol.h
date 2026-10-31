#ifndef CONPTYWORKERPROTOCOL_H
#define CONPTYWORKERPROTOCOL_H
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace process::worker_protocol {
// Little endian, bounded frames. stdout is raw VT; stdin and stderr are protocol only.
constexpr uint32_t magic = 0x57595450;
constexpr uint32_t version = 1;
constexpr uint32_t max_payload = 16 * 1024 * 1024;
enum Type : uint32_t { Init = 1, Input, Resize, CloseInput, Stop, Result };
inline void put(std::string &s, uint32_t n) {
	for (unsigned i = 0; i < 4; ++i) s.push_back(static_cast<char>(n >> (i * 8)));
}
inline uint32_t get(std::string_view &s) {
	if (s.size() < 4) throw std::runtime_error("Truncated worker message");
	uint32_t n = 0;
	for (unsigned i = 0; i < 4; ++i) n |= uint32_t(static_cast<unsigned char>(s[i])) << (i * 8);
	s.remove_prefix(4);
	return n;
}
inline void put_string(std::string &s, std::string_view value) {
	if (value.size() > max_payload) throw std::runtime_error("Worker field too large");
	put(s, static_cast<uint32_t>(value.size())); s.append(value);
}
inline std::string get_string(std::string_view &s) {
	auto n = get(s);
	if (n > s.size()) throw std::runtime_error("Truncated worker field");
	std::string value(s.substr(0, n)); s.remove_prefix(n); return value;
}
inline std::string frame(Type type, std::string_view payload = {}) {
	if (payload.size() > max_payload) throw std::runtime_error("Worker message too large");
	std::string s; put(s, magic); put(s, version); put(s, type);
	put(s, static_cast<uint32_t>(payload.size())); s.append(payload); return s;
}
inline bool take(std::string &buffer, Type &type, std::string &payload) {
	if (buffer.size() < 16) return false;
	std::string_view s(buffer);
	if (get(s) != magic || get(s) != version) throw std::runtime_error("Worker protocol version mismatch");
	auto t = get(s); auto n = get(s);
	if (t < Init || t > Result || n > max_payload) throw std::runtime_error("Invalid worker frame");
	if (buffer.size() - 16 < n) return false;
	type = static_cast<Type>(t); payload.assign(buffer, 16, n); buffer.erase(0, 16 + n); return true;
}
}
#endif
