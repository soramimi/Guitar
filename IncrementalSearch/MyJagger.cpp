#include "MyJagger.h"
#include <string>
#include <jagger.h>

#include <jagger.cc>
#define JAGGER_DEFAULT_MODEL "/home/soramimi/develop/jagger-example/jagger/model/kwdlc"

struct LibJagger::Private {
	jagger::tagger jagger;

};


LibJagger::LibJagger()
	: m(new Private)
{
}

LibJagger::~LibJagger()
{
	delete m;
}

bool LibJagger::open(const char *dicpath)
{
	std::string model(JAGGER_DEFAULT_MODEL "/patterns");
	m->jagger.read_model(model);
	return true;
}

void LibJagger::close()
{

}

std::vector<LibJagger::Part> LibJagger::parse(const std::string_view &line)
{
	std::vector<jagger::tagger::Part> parts = m->jagger.run(line);

	std::vector<Part> ret;
	for (const auto &part : parts) {
		ret.push_back({part.offset, part.length, part.text});
	}

	return ret;
}
