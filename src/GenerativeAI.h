#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <QString>

namespace GenerativeAI {

enum Type {
	Unknown,
	GPT,
	CLAUDE,
};

struct Model {
	QString model;
	Type type = Unknown;
	Model() = default;
	Model(const QString &model)
		: model(model)
	{
		if (model.startsWith("gpt-")) {
			type = GPT;
		} else if (model.startsWith("claude-")) {
			type = CLAUDE;
		}
	}
};

static std::vector<Model> available_models()
{
	std::vector<Model> models;
	models.emplace_back("gpt-3.5-turbo");
	models.emplace_back("gpt-4");
	models.emplace_back("gpt-4-turbo");
	models.emplace_back("gpt-4o");
	models.emplace_back("claude-3-opus-20240229");
	models.emplace_back("claude-3-sonnet-20240229");
	models.emplace_back("claude-3-haiku-20240307");
	return models;
}

} // namespace

#endif // GENERATIVEAI_H
