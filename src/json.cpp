
#include "json.h"
#include <string.h>
#include <stdlib.h>
#include "charvec.h"

int JSON::scan_space(const char *ptr, const char *end)
{
	int i;
	for (i = 0; ptr + i < end && isspace(ptr[i] & 0xff); i++);
	return i;
}

int JSON::parse_string(const char *begin, const char *end, std::string *out)
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
					auto push = [&](char c){ vec.push_back(c); ptr++; };
					switch (*ptr) {
					case 'a': push('\a'); break;
					case 'b': push('\b'); break;
					case 'n': push('\n'); break;
					case 'r': push('\r'); break;
					case 'f': push('\f'); break;
					case 't': push('\t'); break;
					case 'v': push('\v'); break;
					case '\\':
					case '\'':
					case '\"':
						push(*ptr);
						break;
					case 'x':
						ptr++;
						if (ptr + 1 < end && isxdigit(ptr[0] & 0xff) && isxdigit(ptr[1] & 0xff)) {
							char tmp[3];
							tmp[0] = ptr[0];
							tmp[1] = ptr[1];
							tmp[2] = 0;
							vec.push_back((char)strtol(tmp, 0, 16));
							ptr += 2;
						}
						break;
					default:
						if (*ptr >= '0' && *ptr <= '7') {
							int i;
							int v = 0;
							for (i = 0; i < 3; i++) {
								if (ptr + i < end && ptr[i] >= '0' && ptr[i] <= '7') {
									v = v * 8 + (ptr[i] - '0');
								} else {
									break;
								}
							}
							vec.push_back(v);
							ptr += i;
						} else {
							vec.push_back(*ptr);
							ptr++;
						}
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

int JSON::parse_name(const char *begin, const char *end, JSON::Node *node)
{
	char const *ptr = begin;
	ptr += scan_space(ptr, end);
	char const *name = ptr;
	int quote = 0;
	if (*ptr == '\"') {
		quote = *ptr;
		name++;
		ptr++;
	}
	while (ptr < end) {
		if (quote) {
			if (*ptr == quote) {
				if (name < ptr) {
					node->name = std::string(name, ptr);
					ptr++;
					return ptr - begin;
				} else {
					return 0;
				}
			} else {
				ptr++;
			}
		} else if (strchr(":={}[]", *ptr) || isspace(*ptr & 0xff)) {
			if (name < ptr) {
				node->name = std::string(name, ptr);
				return ptr - begin;
			}
			return 0;
		} else {
			ptr++;
		}
	}
	return 0;
}

int JSON::parse_value(const char *begin, const char *end, JSON::Node *node)
{
	char const *ptr = begin;
	int n;
	ptr += scan_space(ptr, end);
	if (ptr < end) {
		if (*ptr == '[') {
			ptr++;
			node->type = Type::Array;
			n = parse_array(ptr, end, false, &node->children);
			ptr += n;
			if (ptr < end && *ptr == ']') {
				ptr++;
				return ptr - begin;
			}
		} else if (*ptr == '{') {
			ptr++;
			node->type = Type::Object;
			n = parse_array(ptr, end, true, &node->children);
			ptr += n;
			if (ptr < end && *ptr == '}') {
				ptr++;
				return ptr - begin;
			}
		} else if (*ptr == '\"') {
			n = parse_string(ptr, end, &node->value);
			if (n > 0) {
				ptr += n;
				node->type = Type::String;
				return ptr - begin;
			}
		} else {
			char const *left = ptr;
			while (ptr < end) {
				int c = *ptr & 0xff;
				if (isspace(c)) break;
				if (strchr("[]{},:\"", c)) break;
				ptr++;
			}
			if (left < ptr) {
				std::string value(left, ptr);
				if (value == "null") {
					node->type = Type::Null;
				} else if (value == "false") {
					node->type = Type::Boolean;
					node->value = "0";
				} else if (value == "true") {
					node->type = Type::Boolean;
					node->value = "1";
				} else {
					node->type = Type::Number;
					node->value = value;
				}
				return ptr - begin;
			}
		}
	}
	return 0;
}

int JSON::parse_array(const char *begin, const char *end, bool withname, std::vector<JSON::Node> *children)
{
	children->clear();
	char const *ptr = begin;
	while (1) {
		int n;
		Node node;
		if (withname) {
			n = parse_name(ptr, end, &node);
			if (n > 0) {
				ptr += n;
				ptr += scan_space(ptr, end);
				if (*ptr == ':') {
					ptr++;
				}
			}
		}
		n = parse_value(ptr, end, &node);
		if (node.type != Type::Unknown) {
			children->push_back(node);
		}
		ptr += n;
		ptr += scan_space(ptr, end);
		if (ptr < end && *ptr == ',') {
			ptr++;
			continue;
		}
		return ptr - begin;
	}
}

bool JSON::parse(const char *begin, const char *end)
{
	return parse_value(begin, end, &node) > 0;
}

bool JSON::parse(std::string const &text)
{
	if (text.empty()) return false;
	const char *begin = text.c_str();
	const char *end = begin + text.size();
	return parse(begin, end);
}

bool JSON::parse(std::vector<char> const *vec)
{
	if (vec->empty()) return false;
	const char *begin = &vec->at(0);
	const char *end = begin + vec->size();
	return parse(begin, end);
}

std::string JSON::double_quoted_string(const std::string &str)
{
	std::vector<char> vec;
	vec.reserve(str.size() + 10);
	vec.push_back('\"');
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;
	auto backslash = [&](char c){
		vec.push_back('\\');
		vec.push_back(c);
	};
	while (ptr < end) {
		switch (*ptr) {
		case '\"':
		case '\\':
		case '/':
			backslash(*ptr);
			break;
		case '\b': backslash('b'); break;
		case '\f': backslash('f'); break;
		case '\n': backslash('n'); break;
		case '\r': backslash('r'); break;
		case '\t': backslash('t'); break;
		default:
			vec.push_back(*ptr);
			break;
		}
		ptr++;
	}
	vec.push_back('\"');
	return to_stdstr(vec);
}

std::string JSON::stringify(const JSON::Node &node, int indent) const
{
	std::string text;
	auto PutIndent = [&](){
		for (int i = 0; i < indent; i++) {
			text += '\t';
		}
	};
	auto PutQuotedString = [&](std::string const &s){
		text += double_quoted_string(s);
	};
	auto PutName = [&](){
		if (!node.name.empty()) {
			PutQuotedString(node.name);
			text += ':';
		}
	};
	auto PutList = [&](char block_begin, char block_end){
		PutIndent();
		PutName();
		text += block_begin;
		text += '\n';
		for (size_t i = 0; i < node.children.size(); i++) {
			text += stringify(node.children[i], indent + 1);
			if (i + 1 < node.children.size()) {
				text += ',';
			}
			text += '\n';
		}
		PutIndent();
		text += block_end;
	};
	if (node.type == Type::Object) {
		PutList('{', '}');
	} else if (node.type == Type::Array) {
		PutList('[', ']');
	} else if (node.type == Type::Number) {
		PutIndent();
		PutName();
		text += node.value;
	} else if (node.type == Type::String) {
		PutIndent();
		PutName();
		PutQuotedString(node.value);
	} else if (node.type == Type::Boolean) {
		PutIndent();
		PutName();
		text += atoi(node.value.c_str()) == 0 ? "false" : "true";
	} else {
		PutIndent();
		PutName();
		text += "null";

	}
	return text;
}

std::string JSON::stringify() const
{
	return stringify(node, 0);
}

JSON::Value JSON::get(const std::string &path) const
{
	std::vector<std::string> vec;
	char const *begin = path.c_str();
	char const *end = begin + path.size();
	char const *ptr = begin;
	char const *left = ptr;
	while (1) {
		int c = -1;
		if (ptr < end) {
			c = *ptr & 0xff;
		}
		if (c == '/' || c == -1) {
			if (left < ptr) {
				std::string s(left, ptr);
				vec.push_back(s);

			}
			if (c == -1) break;
			ptr++;
			left = ptr;
		} else {
			ptr++;
		}
	}
	Node const *p = &node;
	for (std::string const &name : vec) {
		Node const *q = nullptr;
		for (Node const &child : p->children) {
			if (child.name == name) {
				q = &child;
				break;
			}
		}
		if (q) {
			p = q;
		} else {
			break;
		}
	}
	Value v;
	if (p) {
		v = *p;
	}
	return v;
}
