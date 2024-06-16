#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <QString>

enum AI_Type {
	Unknown,
	GPT,
	CLAUDE,
};


class GenerativeAI {
public:
	struct Model {
		QString model;
		Model() = default;
		Model(const QString &model)
			: model(model)
		{
		}
		AI_Type type() const
		{
			if (model.startsWith("gpt-")) {
				return GPT;
			}
			if (model.startsWith("claude-")) {
				return CLAUDE;
			}
			return Unknown;
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
	
};



#endif // GENERATIVEAI_H
