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
};



#endif // GENERATIVEAI_H
