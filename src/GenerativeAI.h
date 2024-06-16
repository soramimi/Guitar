#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <QString>

namespace GenerativeAI {

enum Type {
	Unknown,
	GPT,
	CLAUDE,
	GEMINI,
};

struct Model {
	QString name;
	Model() = default;
	Model(const QString &name)
		: name(name)
	{
	}
	Type type() const
	{
		if (name.startsWith("gpt-")) {
			return GPT;
		}
		if (name.startsWith("claude-")) {
			return CLAUDE;
		}
		if (name.startsWith("gemini-")) {
			return GEMINI;
		}
		return Unknown;
	}
	QString anthropic_version() const
	{
		return "2023-06-01";
	}
};

static std::vector<Model> available_models()
{
	std::vector<Model> models;
	models.emplace_back("gpt-3.5-turbo");
	models.emplace_back("gpt-4");
	models.emplace_back("gpt-4-turbo");
	models.emplace_back("gpt-4o");
	models.emplace_back("claude-3-haiku-20240307");
	models.emplace_back("claude-3-sonnet-20240229");
	models.emplace_back("claude-3-opus-20240229");
	models.emplace_back("gemini-1.0-ultra");
	models.emplace_back("gemini-1.0-pro");
	models.emplace_back("gemini-1.0-flash");
	models.emplace_back("gemini-1.0-nano");
	return models;
}

} // namespace

#endif // GENERATIVEAI_H
