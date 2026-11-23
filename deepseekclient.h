#ifndef DEEPSEEKCLIENT_H
#define DEEPSEEKCLIENT_H

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// DeepSeek Chat Completions 客户端（非流式，一次返回完整回复）
class DeepSeekClient : public QObject
{
    Q_OBJECT

public:
    explicit DeepSeekClient(QObject *parent = nullptr);

    void setApiKey(const QString &apiKey);
    QString apiKey() const;

    void setBaseUrl(const QUrl &url);        // 默认 https://api.deepseek.com/chat/completions
    void setModel(const QString &model);     // 默认 deepseek-chat

    // 发送一次对话请求（system 提示词内部固定为 "You are a helpful assistant."）
    void sendMessage(const QString &prompt,
                     int maxTokens = 2048,
                     double temperature = 0.7);

    void cancel();                           // 中止当前请求

signals:
    void responseReceived(const QString &content);   // 成功拿到完整回复
    void requestFailed(const QString &errorString);  // 请求失败 / 解析失败
    void finished();                                 // 本轮结束（无论成功失败）

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_activeReply = nullptr;

    QString m_apiKey;
    QString m_model = QStringLiteral("deepseek-chat");
    QUrl m_baseUrl = QUrl(QStringLiteral("https://api.deepseek.com/chat/completions"));
};

#endif // DEEPSEEKCLIENT_H