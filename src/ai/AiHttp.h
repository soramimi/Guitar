#pragma once

#include <atomic>
#include <memory>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QTimer>
#include <inet/inetclient.h>
#ifdef APP_GUITAR
#include "ApplicationGlobal.h"
#include <inet/webclient.h>
#endif

// Each request owns its Qt objects in the calling worker thread. TLS errors are
// fatal; redirects are not followed with credentials. Only metadata may be logged.
class AiHttp {
	std::shared_ptr<std::atomic_bool> cancelled_;
	QByteArray body_;
	int timeout_ms_;
	int perform(InetClient::Request const &request, InetClient::Post const *post)
	{
		body_.clear();
		if (cancelled_ && cancelled_->load()) return -1;
		QUrl url(QString::fromStdString(request.url().full_request()));
		if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) return -1;
		QNetworkAccessManager manager;
		QString proxy = qEnvironmentVariable(url.scheme() == "https" ? "HTTPS_PROXY" : "HTTP_PROXY");
		if (proxy.isEmpty()) proxy = qEnvironmentVariable(url.scheme() == "https" ? "https_proxy" : "http_proxy");
#ifdef APP_GUITAR
		if (global && global->webcx()) {
			auto configured = url.scheme() == "https" ? global->webcx()->https_proxy() : global->webcx()->http_proxy();
			if (configured) proxy = QString::fromStdString(configured->server);
		}
#endif
		bool direct = url.host() == "localhost" || url.host() == "127.0.0.1" || url.host() == "::1";
		QString noProxy = qEnvironmentVariable("NO_PROXY", qEnvironmentVariable("no_proxy"));
		for (auto entry : noProxy.split(',')) {
			entry = entry.trimmed();
			if (entry.startsWith('.')) entry.remove(0, 1);
			if (!entry.isEmpty() && (entry == "*" || url.host() == entry || url.host().endsWith('.' + entry))) direct = true;
		}
		if (direct) manager.setProxy(QNetworkProxy::NoProxy);
		else if (!proxy.isEmpty()) {
			QUrl p(proxy);
			manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, p.host(), p.port(8080), p.userName(), p.password()));
		}
		QNetworkRequest req(url);
		req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
		for (auto const &header : request.headers()) {
			auto pos = header.find(':');
			if (pos != std::string::npos)
				req.setRawHeader(QByteArray::fromStdString(header.substr(0, pos)).trimmed(), QByteArray::fromStdString(header.substr(pos + 1)).trimmed());
		}
		QNetworkReply *reply;
		if (post) {
			req.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromStdString(post->content_type));
			reply = manager.post(req, QByteArray(post->data.data(), post->data.size()));
		} else reply = manager.get(req);
		reply->setReadBufferSize(65536);
		QEventLoop loop;
		bool aborted = false;
		auto abort = [&] { aborted = true; reply->abort(); loop.quit(); };
		auto read = [&] {
			auto chunk = reply->readAll();
			if (body_.size() + chunk.size() > 4 * 1024 * 1024) abort();
			else body_ += chunk;
		};
		QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
		QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		QTimer deadline;
		deadline.setSingleShot(true);
		QObject::connect(&deadline, &QTimer::timeout, &loop, abort);
		deadline.start(timeout_ms_);
		QTimer cancellation;
		QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (cancelled_ && cancelled_->load()) abort(); });
		cancellation.start(25);
		if (!reply->isFinished()) loop.exec();
		if (!aborted) read();
		int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (aborted || status == 0 || (reply->error() != QNetworkReply::NoError && status < 400)) {
			body_.clear(); return -1;
		}
		return status;
	}
public:
	explicit AiHttp(std::shared_ptr<std::atomic_bool> cancelled = {}, int timeout_ms = 120000)
		: cancelled_(std::move(cancelled)), timeout_ms_(timeout_ms) {}
	int get(InetClient::Request const &request) { return perform(request, nullptr); }
	int post(InetClient::Request const &request, InetClient::Post const *data) { return perform(request, data); }
	char const *content_data() const { return body_.constData(); }
	size_t content_length() const { return static_cast<size_t>(body_.size()); }
};
