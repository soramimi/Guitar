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
	QString model;
	Model() = default;
	Model(const QString &model)
		: model(model)
	{
	}
	Type type() const
	{
		if (model.startsWith("gpt-")) {
			return GPT;
		}
		if (model.startsWith("claude-")) {
			return CLAUDE;
		}
		if (model.startsWith("gemini-")) {
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
	models.emplace_back("gemini-pro");
	return models;
}

} // namespace

#endif // GENERATIVEAI_H
