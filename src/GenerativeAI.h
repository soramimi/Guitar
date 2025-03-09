#ifndef GENERATIVEAI_H
#define GENERATIVEAI_H

#include <QString>
#include <vector>

namespace GenerativeAI {

enum Type {
	Unknown,
	GPT,
	CLAUDE,
	GEMINI,
	DEEPSEEK,
	OLLAMA,
};

struct Model {
	QString name;
	Model() = default;
	Model(const QString &name);
	Type type() const;
	QString anthropic_version() const;
};

std::vector<Model> available_models();

} // namespace GenerativeAI

#endif // GENERATIVEAI_H
