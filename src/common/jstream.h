#ifndef JSTREAM_H_
#define JSTREAM_H_

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace jstream {

enum Symbol {
	None,
	Null,
	False,
	True,
};

enum StateType {
	Key = 100,
	Comma,
	StartObject,
	EndObject,
	StartArray,
	EndArray,
	String,
	Number,
	Error,
};

class Reader {
private:
	static std::string to_stdstr(std::vector<char> const &vec)
	{
		if (!vec.empty()) {
			char const *begin = &vec[0];
			char const *end = begin + vec.size();
			return std::string(begin, end);
		}
		return std::string();
	}

	static int scan_space(char const *ptr, char const *end)
	{
		int i;
		for (i = 0; ptr + i < end && isspace(ptr[i] & 0xff); i++);
		return i;
	}

	static int parse_symbol(char const *begin, char const *end, std::string *out)
	{
		char const *ptr = begin;
		ptr += scan_space(ptr, end);
		std::vector<char> vec;
		while (ptr < end) {
			if (!isalnum((unsigned char)*ptr)) break;
			vec.push_back(*ptr);
			ptr++;
		}
		if (ptr > begin && !vec.empty()) {
			*out = to_stdstr(vec);
			return ptr - begin;
		}
		out->clear();
		return 0;
	}

	static int parse_number(char const *begin, char const *end, double *out)
	{
		*out = 0;
		char const *ptr = begin;
		ptr += scan_space(ptr, end);
		std::vector<char> vec;
		while (ptr < end) {
			char c = *ptr;
			if (isdigit((unsigned char)c) || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E') {
				// thru
			} else {
				break;
			}
			vec.push_back(c);
			ptr++;
		}
		vec.push_back(0);
		*out = strtod(vec.data(), nullptr);
		return ptr - begin;
	}

	static int parse_string(char const *begin, char const *end, std::string *out)
	{
		char const *ptr = begin;
		ptr += scan_space(ptr, end);
		if (*ptr == '\"') {
			ptr++;
			std::vector<char> vec;
			while (ptr < end) {
				if (*ptr == '\"') {
					*out = to_stdstr(vec);
					ptr++;
					return ptr - begin;
				} else if (*ptr == '\\') {
					ptr++;
					if (ptr < end) {
						auto push = [&](char c){ vec.push_back(c); ptr++;};
						switch (*ptr) {
						case 'b': push('\b'); break;
						case 'n': push('\n'); break;
						case 'r': push('\r'); break;
						case 'f': push('\f'); break;
						case 't': push('\t'); break;
						case '\\':
						case '\"':
							push(*ptr);
							break;
						case 'u':
							ptr++;
							if (ptr + 3 < end) {
								char tmp[5];
								tmp[0] = ptr[0];
								tmp[1] = ptr[1];
								tmp[2] = ptr[2];
								tmp[3] = ptr[3];
								tmp[4] = 0;
								ptr += 4;
								uint32_t unicode = (uint32_t)strtol(tmp, nullptr, 16);
								if (unicode >= 0xd800 && unicode < 0xdc00) {
									if (ptr + 5 < end && ptr[0] == '\\' && ptr[1] == 'u') {
										tmp[0] = ptr[2];
										tmp[1] = ptr[3];
										tmp[2] = ptr[4];
										tmp[3] = ptr[5];
										uint32_t surrogate = (uint32_t)strtol(tmp, nullptr, 16);
										if (surrogate >= 0xdc00 && surrogate < 0xe000) {
											ptr += 6;
											unicode = ((unicode - 0xd800) << 10) + (surrogate - 0xdc00) + 0x10000;
										}
									}
								}
								if (unicode < (1 << 7)) {
									vec.push_back(unicode & 0x7f);
								} else if (unicode < (1 << 11)) {
									vec.push_back(((unicode >> 6) & 0x1f) | 0xc0);
									vec.push_back((unicode & 0x3f) | 0x80);
								} else if (unicode < (1 << 16)) {
									vec.push_back(((unicode >> 12) & 0x0f) | 0xe0);
									vec.push_back(((unicode >> 6) & 0x3f) | 0x80);
									vec.push_back((unicode & 0x3f) | 0x80);
								} else if (unicode < (1 << 21)) {
									vec.push_back(((unicode >> 18) & 0x07) | 0xf0);
									vec.push_back(((unicode >> 12) & 0x3f) | 0x80);
									vec.push_back(((unicode >> 6) & 0x3f) | 0x80);
									vec.push_back((unicode & 0x3f) | 0x80);
								}
							}
							break;
						default:
							vec.push_back(*ptr);
							ptr++;
							break;
						}
					}
				} else {
					vec.push_back(*ptr);
					ptr++;
				}
			}
		}
		return 0;
	}
private:
	struct ParserData {
		char const *begin = nullptr;
		char const *end = nullptr;
		char const *ptr = nullptr;
		bool easy_mode = false;
		std::vector<StateType> states;
		std::string key;
		std::string string;
		double number = 0;
		bool is_array = false;
		std::vector<std::string> depth;
	};
	ParserData d;

	void push_state(StateType s)
	{
		if (state() == Key || state() == Comma || state() == EndObject) {
			d.states.pop_back();
		}
		d.states.push_back(s);

		if (isarray()) {
			d.key.clear();
		}

		switch (s) {
		case StartArray:
			d.is_array = true;
			break;
		case StartObject:
		case Key:
			d.is_array = false;
			break;
		}
	}

	bool pop_state()
	{
		bool f = false;
		if (!d.states.empty()) {
			d.states.pop_back();
			size_t i = d.states.size();
			while (i > 0) {
				i--;
				auto s = d.states[i];
				if (s == StartArray) {
					d.is_array = true;
					break;
				}
				if (s == StartObject || s == Key) {
					d.is_array = false;
					break;
				}
			}
			f = true;
			if (state() == Key) {
				d.states.pop_back();
			}
		}
		d.key.clear();
		return f;
	}

	bool is_easy_mode() const
	{
		return d.easy_mode;
	}

public:
	Reader(char const *begin, char const *end)
	{
		parse(begin, end);
	}
	Reader(char const *ptr, int len = -1)
	{
		parse(ptr, len);
	}
	void set_easy_mode(bool easy)
	{
		d.easy_mode = easy;
	}
	void parse(char const *begin, char const *end)
	{
		d = {};
		d.begin = begin;
		d.end = end;
		d.ptr = d.begin;
	}
	void parse(char const *ptr, int len = -1)
	{
		if (len < 0) {
			len = strlen(ptr);
		}
		parse(ptr, ptr + len);
	}
	bool next()
	{
		d.ptr += scan_space(d.ptr, d.end);
		if (d.ptr < d.end) {
			if (*d.ptr == '}') {
				d.ptr++;
				d.string.clear();
				std::string key;
				if (!d.depth.empty()) {
					key = d.depth.back();
					auto n = key.size();
					if (n > 0) {
						if (key[n - 1] == '{') {
							key = key.substr(0, n - 1);
						}
					}
					d.depth.pop_back();
				}
				while (1) {
					bool f = (state() == StartObject);
					if (!pop_state()) break;
					if (f) {
						push_state(EndObject);
						d.key = key;
						return true;
					}
				}
			}
			if (*d.ptr == ']') {
				d.ptr++;
				d.string.clear();
				std::string key;
				if (!d.depth.empty()) {
					key = d.depth.back();
					auto n = key.size();
					if (n > 0) {
						if (key[n - 1] == '[') {
							key = key.substr(0, n - 1);
						}
					}
					d.depth.pop_back();
				}
				while (1) {
					bool f = (state() == StartArray);
					if (!pop_state()) break;
					if (f) {
						push_state(EndArray);
						d.key = key;
						return true;
					}
				}
			}
			if (*d.ptr == ',') {
				d.ptr++;
				d.ptr += scan_space(d.ptr, d.end);
				if (isobject() || isvalue()) {
					pop_state();
				}
				push_state(Comma);
			}
			if (*d.ptr == '{') {
				d.ptr++;
				if (state() != Key) {
					d.key.clear();
					d.string.clear();
				}
				d.depth.push_back(d.key + '{');
				push_state(StartObject);
				return true;
			}
			if (*d.ptr == '[') {
				d.ptr++;
				if (state() != Key) {
					d.key.clear();
					d.string.clear();
				}
				d.depth.push_back(d.key + '[');
				push_state(StartArray);
				return true;
			}
			if (*d.ptr == '\"') {
				auto n = parse_string(d.ptr, d.end, &d.string);
				if (n > 0) {
					if (isarray()) {
						d.ptr += n;
						push_state(String);
						return true;
					}
					if (state() == Key) {
						d.ptr += n;
						push_state(String);
						return true;
					}
					n += scan_space(d.ptr + n, d.end);
					if (d.ptr[n] == ':') {
						d.ptr += n + 1;
						d.key = d.string;
						push_state(Key);
						return true;
					}
				}
			}
			if (isdigit((unsigned char)*d.ptr) || *d.ptr == '-') {
				auto n = parse_number(d.ptr, d.end, &d.number);
				if (n > 0) {
					d.string.assign(d.ptr, n);
					d.ptr += n;
					push_state(Number);
					return true;
				}
			}
			if (isalpha((unsigned char)*d.ptr)) {
				auto n = parse_symbol(d.ptr, d.end, &d.string);
				if (n > 0) {
					if (state() == Key || state() == Comma || state() == StartArray) {
						d.ptr += n;
						if (d.string == "false") {
							push_state((StateType)False);
							return true;
						}
						if (d.string == "true") {
							push_state((StateType)True);
							return true;
						}
						if (d.string == "null") {
							push_state((StateType)Null);
							return true;
						}
					}
				}
			}
			if (is_easy_mode() && state() == Comma) {
				return true;
			}
			d.states.clear();
			push_state(Error);
			d.string = "syntax error";
		}
		return false;
	}

	StateType state() const
	{
		return d.states.empty() ? (StateType)None : d.states.back();
	}

	bool isobject() const
	{
		switch (state()) {
		case StartObject:
		case StartArray:
			return true;
		case EndObject:
		case EndArray:
			if (d.states.size() > 1) {
				auto s = d.states[d.states.size() - 2];
				switch (s) {
				case StartObject:
				case StartArray:
					return true;
				}
			}
			break;
		}
		return false;
	}

	bool isvalue() const
	{
		switch (state()) {
		case String:
		case Number:
		case Null:
		case False:
		case True:
			return true;
		}
		return false;
	}

	std::string key() const
	{
		return d.key;
	}

	std::string string() const
	{
		return d.string;
	}

	Symbol symbol() const
	{
		int s = state();

		switch (s) {
		case Null:
		case False:
		case True:
			return (Symbol)s;
		}
		return None;
	}

	bool isnull() const
	{
		return symbol() == Null;
	}

	bool isfalse() const
	{
		return symbol() == False;
	}

	bool istrue() const
	{
		return symbol() == True;
	}

	double number() const
	{
		return d.number;
	}

	bool isarray() const
	{
		return d.is_array;
	}

	int depth() const
	{
		return d.depth.size();
	}

	std::string path() const
	{
		std::string path;
		for (std::string const &s : d.depth) {
			path += s;
		}
		return path + d.key;
	}

	bool match(char const *path, std::vector<std::string> *vals = nullptr) const
	{
		if (vals) {
			vals->clear();
		}
		if (!(isobject() || isvalue())) return false;
		size_t i;
		for (i = 0; i < d.depth.size(); i++) {
			std::string const &s = d.depth[i];
			if (s.empty()) break;
			if (path[0] == '*' && (path[1] == 0 || path[1] == '{') && s.c_str()[s.size() - 1] == '{') {
				std::string t;
				if (path[1] == 0) {
					if (i + 1 == d.depth.size()) {
						if (vals) {
							while (i < d.depth.size()) {
								t += d.depth[i];
								i++;
							}
							if (isvalue()) {
								t += d.key;
							}
							vals->push_back(t);
						}
						return true;
					}
					return false;
				}
				t = s.substr(0, s.size() - 1);
				if (vals) {
					vals->push_back(t);
				}
				path += 2;
				continue;
			}
			if (strncmp(path, s.c_str(), s.size()) != 0) return false;
			path += s.size();
		}
		if (path[0] == '*') {
			if (path[1] == 0 && i == d.depth.size() && (isvalue() || state() == EndObject || state() == EndArray)) {
				if (vals) {
					std::string t;
					if (isvalue()) {
						t = d.key;
					}
					vals->push_back(t);
				}
				return true;
			}
			return false;
		}
		return path == d.key;
	}
};

class Writer {
protected:
	void print(char const *p, int n)
	{
		if (output_fn) {
			output_fn(p, n);
		} else {
			for (size_t i = 0; i < n; i++) {
				putchar(p[i]);
			}
		}
	}

	void print(char c)
	{
		print(&c, 1);
	}

	void print(char const *p)
	{
		print(p, strlen(p));
	}

	void print(std::string const &s)
	{
		print(s.c_str(), s.size());
	}
private:
	std::vector<int> stack;
	std::function<void (char const *p, int n)> output_fn;

	void printIndent()
	{
		size_t n = stack.size() - 1;
		for (size_t i = 0; i < n; i++) {
			print(' ');
			print(' ');
		}
	}

	void printNumber(double v)
	{
		char tmp[100];
		sprintf(tmp, "%f", v);
		char *ptr = strchr(tmp, '.');
		if (ptr) {
			char *end = ptr + strlen(ptr);
			while (ptr < end && end[-1] == '0') {
				end--;
			}
			if (ptr + 1 == end) {
				end--;
			}
			*end = 0;
		}
		print(tmp);
	}

	void printString(std::string const &s)
	{
		char const *ptr = s.c_str();
		char const *end = ptr + s.size();
		std::vector<char> buf;
		buf.reserve(end - ptr + 10);
		while (ptr < end) {
			int c = (unsigned char)*ptr;
			ptr++;
			switch (c) {
			case '\"': buf.push_back('\\'); buf.push_back('\"'); break;
			case '\\': buf.push_back('\\'); buf.push_back('\\'); break;
			case '\b': buf.push_back('\\'); buf.push_back('b'); break;
			case '\f': buf.push_back('\\'); buf.push_back('f'); break;
			case '\n': buf.push_back('\\'); buf.push_back('n'); break;
			case '\r': buf.push_back('\\'); buf.push_back('r'); break;
			case '\t': buf.push_back('\\'); buf.push_back('t'); break;
			default:
				if (c >= 0x20 && c < 0x7f) {
					buf.push_back(c);
				} else {
					uint32_t u = 0;
					if ((c & 0xe0) == 0xc0 && ptr < end) {
						if ((ptr[0] & 0xc0) == 0x80) {
							int d = (unsigned char)ptr[0];
							u = ((c & 0x1f) << 6) | (d & 0x3f);
						}
					} else if ((c & 0xf0) == 0xe0 && ptr + 1 < end) {
						if ((ptr[0] & 0xc0) == 0x80 && (ptr[1] & 0xc0) == 0x80) {
							int d = (unsigned char)ptr[0];
							int e = (unsigned char)ptr[1];
							u = ((c & 0x0f) << 12) | ((d & 0x3f) << 6) | (e & 0x3f);
						}
					} else if ((c & 0xf8) == 0xf0 && ptr + 2 < end) {
						if ((ptr[0] & 0xc0) == 0x80 && (ptr[1] & 0xc0) == 0x80 && (ptr[2] & 0xc0) == 0x80) {
							int d = (unsigned char)ptr[0];
							int e = (unsigned char)ptr[1];
							int f = (unsigned char)ptr[2];
							u = ((c & 0x0f) << 18) | ((d & 0x3f) << 12) | ((e & 0x3f) << 6) | (f & 0x3f);
						}
					}
					if (u != 0) {
						char tmp[20];
						if (u >= 0x10000 && u < 0x110000) {
							uint16_t h = (u - 0x10000) / 0x400 + 0xd800;
							uint16_t l = (u - 0x10000) % 0x400 + 0xdc00;
							sprintf(tmp, "\\u%04X\\u%04X", h, l);
							buf.insert(buf.end(), tmp, tmp + 12);
						} else {
							sprintf(tmp, "\\u%04X", u);
							buf.insert(buf.end(), tmp, tmp + 6);
						}
					}
				}
			}
		}
		print('\"');
		if (!buf.empty()) {
			print(buf.data(), buf.size());
		}
		print('\"');
	}

	void printValue(std::string const &name, std::function<void ()> const &fn)
	{
		printName(name);

		fn();

		if (!stack.empty()) stack.back()++;
	}

	void printObject(std::string const &name = {}, std::function<void ()> const &fn = {})
	{
		printName(name);
		print('{');
		stack.push_back(0);
		if (fn) {
			fn();
			endObject();
		}
	}

	void printArray(std::string const &name = {}, std::function<void ()> const &fn = {})
	{
		printName(name);
		print('[');
		stack.push_back(0);
		if (fn) {
			fn();
			endArray();
		}
	}

	void endBlock()
	{
		print('\n');
		if (!stack.empty()) {
			stack.pop_back();
			if (!stack.empty()) stack.back()++;
		}
		printIndent();
	}
public:
	Writer(std::function<void (char const *p, int n)> fn = {})
	{
		output_fn = fn;
		stack.push_back(0);
	}

	~Writer()
	{
		if (!stack.empty() && stack.front() > 0) {
			print('\n');
		}
	}

	void printName(std::string const &name)
	{
		if (!stack.empty()) {
			if (stack.back() > 0) {
				print(',');
			}
		}
		if (stack.size() > 1) {
			print('\n');
		}
		printIndent();
		if (!name.empty()) {
			printString(name);
			print(':');
			print(' ');
		}
	}

	void startObject(std::string const &name = {})
	{
		printObject(name);
	}

	void endObject()
	{
		endBlock();
		print('}');
	}

	void object(std::string const &name, std::function<void ()> const &fn)
	{
		printObject(name, fn);
	}

	void startArray(std::string const &name = {})
	{
		printArray(name, {});
	}

	void endArray()
	{
		endBlock();
		print(']');
	}

	void array(std::string const &name, std::function<void ()> const &fn)
	{
		printArray(name, fn);
	}

	void number(double v, std::string const &name = {})
	{
		printValue(name, [&](){
			printNumber(v);
		});
	}

	void string(std::string const &s, std::string const &name = {})
	{
		printValue(name, [&](){
			printString(s);
		});
	}

	void symbol(Symbol v, std::string const &name = {})
	{
		printValue(name, [&](){
			switch (v) {
				break;
			case False:
				print("false");
				break;
			case True:
				print("true");
				break;
			default:
				print("null");
			}
		});
	}

	void boolean(bool b, std::string const &name = {})
	{
		symbol(b ? True : False, name);
	}

	void null()
	{
		symbol(Null);
	}
};

} // namespace jstream

#endif // JSTREAM_H_
