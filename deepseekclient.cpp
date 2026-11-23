#include "deepseekclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

DeepSeekClient::DeepSeekClient(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &DeepSeekClient::onReplyFinished);
}

void DeepSeekClient::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
}

QString DeepSeekClient::apiKey() const
{
    return m_apiKey;
}

void DeepSeekClient::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

void DeepSeekClient::setModel(const QString &model)
{
    m_model = model;
}

void DeepSeekClient::sendMessage(const QString &prompt, int maxTokens, double temperature)
{
    if (m_apiKey.isEmpty()) {
        emit requestFailed(QStringLiteral("API Key 为空，请先填写 sk-xxxxx"));
        emit finished();
        return;
    }
    if (prompt.trimmed().isEmpty()) {
        emit requestFailed(QStringLiteral("提示词不能为空"));
        emit finished();
        return;
    }

    // 构造请求体，和你参考代码一致
    QJsonObject requestData;
    requestData[QStringLiteral("model")] = m_model;
    requestData[QStringLiteral("stream")] = false;      // 非流式
    requestData[QStringLiteral("max_tokens")] = maxTokens;
    requestData[QStringLiteral("temperature")] = temperature;

    QJsonArray messages;

    QJsonObject systemMessage;
    systemMessage[QStringLiteral("role")] = QStringLiteral("system");
    systemMessage[QStringLiteral("content")] = QStringLiteral("You are a helpful assistant.");
    messages.append(systemMessage);

    QJsonObject userMessage;
    userMessage[QStringLiteral("role")] = QStringLiteral("user");
    userMessage[QStringLiteral("content")] = prompt;
    messages.append(userMessage);

    requestData[QStringLiteral("messages")] = messages;

    QNetworkRequest request(m_baseUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_apiKey.toUtf8());
    request.setRawHeader("Accept", "application/json");

    m_activeReply = m_networkManager->post(
        request, QJsonDocument(requestData).toJson(QJsonDocument::Compact));
}

void DeepSeekClient::cancel()
{
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
}

void DeepSeekClient::onReplyFinished(QNetworkReply *reply)
{
    if (m_activeReply == reply) {
        m_activeReply = nullptr;
    }

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray responseData = reply->readAll();

        QJsonParseError parseError;
        const QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            emit requestFailed(QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString()));
            reply->deleteLater();
            emit finished();
            return;
        }

        const QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains(QStringLiteral("choices"))) {
            const QJsonArray choices = jsonObject[QStringLiteral("choices")].toArray();
            if (!choices.isEmpty()) {
                const QJsonObject firstChoice = choices.first().toObject();
                const QString content = firstChoice[QStringLiteral("message")]
                                            .toObject()[QStringLiteral("content")].toString();
                emit responseReceived(content);
            } else {
                emit requestFailed(QStringLiteral("响应中 choices 为空"));
            }
        } else {
            // DeepSeek 报错时通常返回 {"error": {"message": "..."}}
            const QString apiError = jsonObject[QStringLiteral("error")]
                                         .toObject()[QStringLiteral("message")].toString();
            emit requestFailed(apiError.isEmpty()
                                   ? QStringLiteral("响应中缺少 choices 字段")
                                   : apiError);
        }
    } else {
        emit requestFailed(reply->errorString());
    }

    reply->deleteLater();
    emit finished();
}