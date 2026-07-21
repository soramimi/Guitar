#ifndef GITHASH_H
#define GITHASH_H

#include <stdint.h>
#include <string>

static constexpr int GIT_ID_LENGTH = 40;

class GitHash {
private:
	bool valid_ = false;
	uint8_t id_[GIT_ID_LENGTH / 2];
	template <typename VIEW> void _assign(VIEW const &id);
public:
	GitHash();
	explicit GitHash(std::string_view const &id);
	explicit GitHash(char const *id);
	void assign(std::string_view const &id);
	std::string toString(int maxlen = -1) const;
	bool isValid() const;
	int compare(GitHash const &other) const;
	explicit operator bool () const;
	operator std::string() const
	{
		return toString();
	}
	size_t _std_hash() const;

	static bool isValidID(std::string const &id);

	static bool isValidID(GitHash const &id)
	{
		return id.isValid();
	}
};

static inline bool operator == (GitHash const &l, GitHash const &r)
{
	return l.compare(r) == 0;
}

static inline bool operator != (GitHash const &l, GitHash const &r)
{
	return l.compare(r) != 0;
}

static inline bool operator < (GitHash const &l, GitHash const &r)
{
	return l.compare(r) < 0;
}

#endif // GITHASH_H
