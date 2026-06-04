#ifndef AIAPIBRIDGE_H
#define AIAPIBRIDGE_H

#include "GenerativeAI.h"

#include <string>
#include <vector>
#include <optional>
#include <functional>

class AbstractInetClient;

/* example for Anthropic Claude responses API

{
  "model": "claude-sonnet-4-6",
  "id": "msg_01QgGhfNSoB4mDVt55i55LXr",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "Sure! Let me fetch today's quote for you right away!"
    },
    {
      "type": "tool_use",
      "id": "toolu_01USQo1v4iheZJ4CxkDxV1ku",
      "name": "get_quote_of_the_day",
      "input": {},
      "caller": {
        "type": "direct"
      }
    }
  ],
  "stop_reason": "tool_use",
  "stop_sequence": null,
  "stop_details": null,
  "usage": {
    "input_tokens": 354,
    "cache_creation_input_tokens": 0,
    "cache_read_input_tokens": 0,
    "cache_creation": {
      "ephemeral_5m_input_tokens": 0,
      "ephemeral_1h_input_tokens": 0
    },
    "output_tokens": 47,
    "service_tier": "standard",
    "inference_geo": "global"
  }
}

*/

/* example for OpenAI responses API

{
  "id": "resp_0e4c24cb1691fbd5006a1d80ad01fc819a93ff21b72cd42581",
  "object": "response",
  "created_at": 1780318381,
  "status": "completed",
  "background": false,
  "billing": {
    "payer": "developer"
  },
  "completed_at": 1780318381,
  "error": null,
  "frequency_penalty": 0.0,
  "incomplete_details": null,
  "instructions": null,
  "max_output_tokens": null,
  "max_tool_calls": null,
  "model": "gpt-5.4-mini-2026-03-17",
  "moderation": null,
  "output": [
    {
      "id": "fc_0e4c24cb1691fbd5006a1d80ad7c20819a9ee90cf849c1a37e",
      "type": "function_call",
      "status": "completed",
      "arguments": "{}",
      "call_id": "call_L8UWZP0N2eEkgc9F0Z9VjKcZ",
      "name": "get_quote_of_the_day"
    }
  ],
  "parallel_tool_calls": true,
  "presence_penalty": 0.0,
  "previous_response_id": null,
  "prompt_cache_key": null,
  "prompt_cache_retention": "in_memory",
  "reasoning": {
    "context": "current_turn",
    "effort": "none",
    "summary": null
  },
  "safety_identifier": null,
  "service_tier": "default",
  "store": true,
  "temperature": 1.0,
  "text": {
    "format": {
      "type": "text"
    },
    "verbosity": "medium"
  },
  "tool_choice": {
    "type": "function",
    "name": "get_quote_of_the_day"
  },
  "tools": [
    {
      "type": "function",
      "description": "Get a quote of the day",
      "name": "get_quote_of_the_day",
      "parameters": {
        "additionalProperties": false,
        "type": "object",
        "properties": {},
        "required": []
      },
      "strict": true
    }
  ],
  "top_logprobs": 0,
  "top_p": 0.98,
  "truncation": "disabled",
  "usage": {
    "input_tokens": 48,
    "input_tokens_details": {
      "cached_tokens": 0
    },
    "output_tokens": 17,
    "output_tokens_details": {
      "reasoning_tokens": 0
    },
    "total_tokens": 65
  },
  "user": null,
  "metadata": {}
}

*/

struct AiResponseEx {
	GenerativeAI::ProviderID api_id = GenerativeAI::ProviderID::Unknown;

	struct AnthropicContentItem {
		std::string type;
		std::string text;
		std::string id;
		std::string name;
		// std::string input;
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

	AiResult(GenerativeAI::ProviderID api = GenerativeAI::ProviderID::Unknown)
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
	std::string const &content() const
	{
		return d.content;
	}
	std::string const &error_status() const
	{
		return d.error_status;
	}
	std::string const &error_message() const
	{
		return d.error_message;
	}
	
	bool is_error() const
	{
		return !d.error_status.empty() || !d.error_message.empty();
	}
};

class AiApiBridge {
	friend class AiSession;
public:
	struct Quert2Resuest {
		GenerativeAI::EndPoint::Type eptype = GenerativeAI::EndPoint::Type::Chat;
		enum Type {
			TEXT,
			JSON,
		};
		Type type = TEXT;
		std::string prompt;
		void set_text(std::string const &text)
		{
			type = TEXT;
			prompt = text;
		}
		void set_json(std::string const &json)
		{
			type = JSON;
			prompt = json;
		}
		Quert2Resuest() = default;
		Quert2Resuest(GenerativeAI::EndPoint::Type eptype, Type t, std::string const &p)
			: eptype(eptype)
			, type(t)
			, prompt(p)
		{
		}
		operator bool () const
		{
			return !prompt.empty();
		}
	};
private:
	struct Private;
	Private *m;
	AbstractInetClient *http();
	std::string generate_prompt_json(const GenerativeAI::Model &model, const std::string &prompt, std::string const &system_role = {});
	AiResult open();
	AiResult x_request(Quert2Resuest const &req);
	void close();
public:
	AiApiBridge();
	~AiApiBridge();
	
	AiResult Error(std::string const &status, std::string const &message) const
	{
		AiResult ret{model().api_compatibility()};
		ret.d.error_status = status;
		ret.d.error_message = message;
		return ret;
	}
	GenerativeAI::Model model() const;
	void set_ai_model(GenerativeAI::Model model);
	void set_system_role(std::string const &role);
	AiResult request(GenerativeAI::EndPoint::Type eptype, std::string const &prompt, const Quert2Resuest &req);
	AiResult request(const std::string &prompt);
	std::optional<AiResult::Models> queryModels();
};


#include <memory>

class AiSession {
public:
	using Quert2Resuest = AiApiBridge::Quert2Resuest;
	std::shared_ptr<AiApiBridge> api_bridge;
	AiSession()
		: api_bridge(std::make_shared<AiApiBridge>())
	{
	}
	~AiSession()
	{
		close();
	}
	void set_ai_model(GenerativeAI::Model model)
	{
		api_bridge->set_ai_model(model);
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
