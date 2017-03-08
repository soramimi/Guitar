#ifndef JSON_H_
#define JSON_H_

#include <string>
#include <vector>

class JSON {
public:
	enum class Type {
		Unknown,
		Null,
		Number,
		String,
		Boolean,
		Array,
		Object,
	};

	class Value {
	public:
		Type type = Type::Unknown;
		std::string name;
		std::string value;
	};

	class Node : public Value {
	public:
		std::vector<Node> children;
	};

	Node node;
private:
	static int scan_space(char const *ptr, char const *end);
	static int parse_string(char const *begin, char const *end, std::string *out);
	static int parse_name(char const *begin, char const *end, Node *node);
	static int parse_value(char const *begin, char const *end, Node *node);
	static int parse_array(char const *begin, char const *end, bool withname, std::vector<Node> *children);
	static std::string double_quoted_string(const std::string &str);
	std::string stringify(const Node &node, int indent) const;
public:
	bool parse(char const *begin, char const *end);
	bool parse(const std::string &text);
	std::string stringify() const;
	Value get(std::string const &path) const;
};

#endif // JSON_H_
