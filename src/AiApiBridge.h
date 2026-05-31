#ifndef AIAPIBRIDGE_H
#define AIAPIBRIDGE_H

#include "GenerativeAI.h"

#include <string>
#include <vector>
#include <optional>
#include <functional>

class AbstractInetClient;

struct AiResponseEx {
	std::string model;
	std::string id;
	std::string type;
	std::string role;
	struct ContentItem {
		std::string type;
		std::string text;
		std::string id;
		std::string name;
		// std::string input;
		std::string caller_type;
	};
	std::vector<ContentItem> content;
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
		bool completion = false;   ///< 正常に完了したか
		std::string content;          ///< AIが返したテキスト本文
		std::string error_status;  ///< エラー種別
		std::string error_message; ///< エラーメッセージ
		AiResponseEx ex;
	} d;

	operator bool () const
	{
		return d.completion && d.error_status.empty() && d.error_message.empty();
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
public:
	struct Quert2Resuest {
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
		Quert2Resuest(Type t, std::string const &p)
			: type(t)
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
	AiResult x_connect();
	AiResult x_query(std::function<Quert2Resuest ()> fn_prompt);
	void x_disconnect();
public:
	AiApiBridge();
	~AiApiBridge();
	
	static AiResult Error(std::string const &status, std::string const &message)
	{
		AiResult ret;
		ret.d.error_status = status;
		ret.d.error_message = message;
		return ret;
	}
	GenerativeAI::Model model();
	void set_ai_model(GenerativeAI::Model model);
	void set_system_role(std::string const &role);
	AiResult query(GenerativeAI::EndPoint::Type eptype, std::string const &prompt);
	AiResult query(const std::string &prompt);
	std::optional<AiResult::Models> queryModels();
	
	AiResult query2(std::function<Quert2Resuest ()> fn_prompt);
};

#endif // AIAPIBRIDGE_H
