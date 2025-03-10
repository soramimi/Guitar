#include "GenerativeAI.h"

std::vector<GenerativeAI::Model> GenerativeAI::available_models()
{
	std::vector<Model> models;
	models.emplace_back("gpt-4o");
	models.emplace_back("gpt-4o-mini");
	models.emplace_back("gpt-4-turbo");
	models.emplace_back("gpt-4");
	models.emplace_back("gpt-3.5-turbo");
	models.emplace_back("claude-3-7-sonnet-latest");
	models.emplace_back("claude-3-5-sonnet-latest");
	models.emplace_back("claude-3-opus-20240229");
	models.emplace_back("claude-3-sonnet-20240229");
	models.emplace_back("claude-3-haiku-20240307");
	models.emplace_back("gemini-1.0-ultra");
	models.emplace_back("gemini-1.0-pro");
	models.emplace_back("gemini-1.0-flash");
	models.emplace_back("gemini-1.0-nano");
	models.emplace_back("deepseek-chat");
	models.emplace_back("ollama-gemma2");
	models.emplace_back("openrouter-deepseek/deepseek-r1:free");
	return models;
}

GenerativeAI::Model::Model(const QString &name)
	: name(name)
{
}

GenerativeAI::Type GenerativeAI::Model::type() const
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
	if (name.startsWith("deepseek-")) {
		return DEEPSEEK;
	}
	if (name.startsWith("ollama-")) {
		return OLLAMA;
	}
	if (name.startsWith("openrouter-")) {
		return OPENROUTER;
	}
	return Unknown;
}

QString GenerativeAI::Model::anthropic_version() const
{
	return "2023-06-01"; // ref. https://docs.anthropic.com/en/api/versioning
}
