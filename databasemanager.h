#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

/*
    DatabaseManager
    负责聊天历史的 SQLite 存取：
    - models 表：模型登记（本地 / 外接）
    - chat_history 表：统一历史记录
    所有 SQL 均使用参数绑定，避免拼接 SQL。
*/

struct ModelRecord {
    int id = -1;
    QString type;
    QString name;
    QString path;
    QString endpoint;
    QString apiModel;
    QString createdAt;
};

struct HistoryRecord {
    int id = -1;
    int modelId = -1;
    QString modelName;
    QString modelType;
    QString modelPath;
    QString prompt;
    QString answer;
    QString status = QStringLiteral("success");
    double temperature = 0.0;
    double topP = 0.0;
    int topK = 0;
    double repeatPenalty = 0.0;
    int seed = -1;
    int maxTokens = 0;
    int contextSize = 0;
    int threads = 0;
    int promptTokens = 0;
    int answerTokens = 0;
    int durationMs = 0;
    QString createdAt;
};

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool open(QString *errorMessage = nullptr);
    void close();
    bool isOpen() const;

    bool initSchema(QString *errorMessage = nullptr);

    // 模型登记：本地按 path 去重，外接按 endpoint+api_model 去重
    int ensureModel(const QString &type,
                    const QString &name,
                    const QString &path = QString(),
                    const QString &endpoint = QString(),
                    const QString &apiModel = QString(),
                    QString *errorMessage = nullptr);

    bool insertHistory(const HistoryRecord &rec, QString *errorMessage = nullptr);

    // 按关键词模糊查询（匹配 prompt / answer / 模型名）
    QVector<HistoryRecord> queryHistory(const QString &searchText = QString(),
                                        QString *errorMessage = nullptr) const;

    bool getHistory(int id, HistoryRecord &out, QString *errorMessage = nullptr) const;

    bool deleteHistory(int id, QString *errorMessage = nullptr);
    bool clearHistory(QString *errorMessage = nullptr);

private:
    QString databasePath() const;
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H