#ifndef AIAPIBRIDGE_H
#define AIAPIBRIDGE_H

#include <ai/GenerativeAI.h>

#include <string>
#include <vector>
#include <optional>
#include <functional>

class AbstractInetClient;

struct AiResponseEx {
	GenerativeAI::ProviderID api_id = GenerativeAI::ProviderID::Custom;

	struct AnthropicContentItem {
		std::string type;
		std::string text;
		std::string id;
		std::string name;
		std::string caller_type;
		std::string content_json;
	};
	
	struct OpenAiOutputItem {
		std::string id;
		std::string type;
		std::string status;
		std::string arguments;
		std::string call_id;
		std::string name;
		struct Content {
			std::string text;
		};
		std::vector<Content> content;
	};
	
	struct GoogleContentItem {
		struct FunctionCall {
			std::string name;
			// std::vector<Args> args;
			std::string id;
			std::string content_json;
			std::string text;
		} functionCall;
	};
	
	struct OpenAiChoice {
		double index;
		struct Message {
			std::string role;
			std::string content;
			struct ToolCall {
				std::string id;
				std::string type;
				struct {
					std::string name;
					std::string arguments;
				} function;
			};
			std::vector<ToolCall> tool_calls;
		} message;
		std::string finish_reason;
	};
	
	std::string model;
	std::string id;
	struct {
		std::string type;
		std::string role;
		std::vector<AnthropicContentItem> content;
	} anthropic;
	struct {
		std::string object;
		std::string status;
		std::vector<OpenAiOutputItem> output;
		std::vector<OpenAiChoice> choices;
	} openai;
	struct {
		std::vector<GoogleContentItem> content_parts;
	} google;
	std::string stop_reason;
	// std::string stop_sequence;
	// std::string stop_details;
	struct Usage {
		int input_tokens;
		int cache_creation_input_tokens;
		int cache_read_input_tokens;
		struct CacheCreation {
			int ephemeral_5m_input_tokens;
			int ephemeral_1h_input_tokens;
		} cache_creation;
		int output_tokens;
		std::string service_tier;
		std::string inference_geo;
	} usage;
	struct Error {
		std::string type;
		std::string message;
	} error;
};

/// AIレスポンスの解析結果を保持する内部構造体
struct AiResult {

	AiResult(GenerativeAI::ProviderID api = GenerativeAI::ProviderID::Custom)
	{
		d.ex.api_id = api;
	}

	struct Model {
		std::string id;
		std::string object;
		std::string created;
		std::string owned_by;
	};

	struct Models {
		std::vector<Model> list;
	};

	struct Data {
		bool completed = false;    ///< 正常に完了したか
		std::string content;       ///< AIが返したテキスト本文
		std::string error_status;  ///< エラー種別
		std::string error_message; ///< エラーメッセージ
		std::string stop_reason;
		AiResponseEx ex;
	} d;

	operator bool () const
	{
		return d.completed && d.error_status.empty() && d.error_message.empty();
	}
	std::string content() const
	{
		return d.content;
	}
	std::string error_status() const
	{
		return d.error_status;
	}
	std::string error_message() const
	{
		return d.error_message;
	}
	
	bool is_error() const
	{
		return !d.error_status.empty() || !d.error_message.empty();
	}
	
	static AiResult Error(std::string const &stat, std::string const &msg)
	{
		AiResult ret;
		ret.d.completed = false;
		ret.d.error_status = stat;
		ret.d.error_message = msg;
		return ret;
	}
	
};

class AiApiBridge {
	friend class AiSession;
public:
	struct Query2Request {
		GenerativeAI::EndPoint::Type eptype = GenerativeAI::EndPoint::Type::Chat;
		enum Type {
			TEXT,
			JSON,
		};
		Type type = TEXT;
		bool internal = false;
		std::string prompt_text;
		std::string prompt_json;
		std::string cursor;
		void set_text(std::string const &text)
		{
			type = TEXT;
			prompt_text = text;
		}
		void set_tooluse(std::string const &json, std::string const &text)
		{
			type = JSON;
			prompt_json = json;
			prompt_text = text;
		}
		Query2Request() = default;
		Query2Request(GenerativeAI::EndPoint::Type eptype)
			: eptype(eptype)
		{
		}
		operator bool () const
		{
			return (type == TEXT && !prompt_text.empty()) || (type == JSON && !prompt_json.empty());
		}
	};
private:
	struct Private;
	Private *m;
	AbstractInetClient *http();
	std::string generate_prompt_json(const GenerativeAI::Model &model, const std::string &prompt, std::string const &system_role = {});
	AiResult open();
	AiResult x_request(Query2Request const &req);
	void close();
public:
	AiApiBridge();
	AiApiBridge(GenerativeAI::Model model, GenerativeAI::Credential cred);
	~AiApiBridge();
	
	AiResult Error(std::string const &status, std::string const &message) const
	{
		AiResult ret{model().api_compatibility()};
		ret.d.error_status = status;
		ret.d.error_message = message;
		return ret;
	}
	GenerativeAI::Model model() const;
	void set_ai_model(GenerativeAI::Model model, GenerativeAI::Credential cred);
	void set_system_role(std::string const &role);
	AiResult request(GenerativeAI::EndPoint::Type eptype, std::string const &prompt, const Query2Request &req);
	AiResult request(const std::string &prompt);
	std::optional<AiResult::Models> queryModels();

	static GenerativeAI::Credential default_credential(GenerativeAI::Model model);
};


#include <memory>

class AiSession {
public:
	using Quert2Resuest = AiApiBridge::Query2Request;
	std::shared_ptr<AiApiBridge> api_bridge;
	AiSession()
		: api_bridge(std::make_shared<AiApiBridge>())
	{
	}
	~AiSession()
	{
		close();
	}
	void set_ai_model(GenerativeAI::Model model, GenerativeAI::Credential cred)
	{
		api_bridge->set_ai_model(model, cred);
	}
	bool open()
	{
		AiResult result = api_bridge->open();
		return !result.is_error();
	}
	void close()
	{
		api_bridge->close();
	}
	AiResult request(Quert2Resuest const &req)
	{
		return api_bridge->x_request(req);
	}
};

#endif // AIAPIBRIDGE_H
