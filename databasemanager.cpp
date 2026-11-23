#include "databasemanager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QCoreApplication>
namespace {

// 空字符串按 NULL 写入，保持“本地模型 API 为空、外接模型路径为空”的约定
QVariant textOrNull(const QString &s)
{
    return s.isEmpty() ? QVariant(QVariant::String) : QVariant(s);
}

QString modelTableSql()
{
    return QStringLiteral(
        "CREATE TABLE IF NOT EXISTS models ("
        " id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        " type       TEXT NOT NULL CHECK (type IN ('local','external')),"
        " name       TEXT NOT NULL,"
        " path       TEXT,"
        " endpoint   TEXT,"
        " api_model  TEXT,"
        " created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now','localtime'))"
        ")");
}

QString historyTableSql()
{
    return QStringLiteral(
        "CREATE TABLE IF NOT EXISTS chat_history ("
        " id             INTEGER PRIMARY KEY AUTOINCREMENT,"
        " model_id       INTEGER NOT NULL REFERENCES models(id),"
        " prompt         TEXT NOT NULL,"
        " answer         TEXT NOT NULL,"
        " status         TEXT NOT NULL DEFAULT 'success',"
        " temperature    REAL,"
        " top_p          REAL,"
        " top_k          INTEGER,"
        " repeat_penalty REAL,"
        " seed           INTEGER,"
        " max_tokens     INTEGER,"
        " context_size   INTEGER,"
        " threads        INTEGER,"
        " prompt_tokens  INTEGER,"
        " answer_tokens  INTEGER,"
        " duration_ms    INTEGER,"
        " created_at     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now','localtime'))"
        ")");
}

} // namespace

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    close();
}

QString DatabaseManager::databasePath() const
{
    // exe 所在目录（Debug/Release 构建输出目录）
    const QString dir = QCoreApplication::applicationDirPath();
    return dir + QStringLiteral("/chat_history.db");
}

bool DatabaseManager::open(QString *errorMessage)
{
    if (m_db.isOpen()) return true;

    const QString path = databasePath();
    if (path.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("无法确定数据库存放目录。");
        return false;
    }

    // 确保目录存在
    const QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    // 使用默认连接，若已存在则直接复用，避免重复 addDatabase 告警
    const QString connectionName = QLatin1String(QSqlDatabase::defaultConnection);
    if (QSqlDatabase::contains(connectionName)) {
        m_db = QSqlDatabase::database(connectionName, false);
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    }

    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        if (errorMessage) *errorMessage = m_db.lastError().text();
        return false;
    }

    // 开启外键约束
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    return initSchema(errorMessage);
}

void DatabaseManager::close()
{
    if (m_db.isValid() && m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::initSchema(QString *errorMessage)
{
    const QStringList statements = {
        modelTableSql(),
        historyTableSql(),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_history_model_id ON chat_history(model_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_history_created_at ON chat_history(created_at)")
    };

    for (const QString &sql : statements) {
        QSqlQuery q(m_db);
        if (!q.exec(sql)) {
            if (errorMessage) *errorMessage = q.lastError().text();
            return false;
        }
    }
    return true;
}

int DatabaseManager::ensureModel(const QString &type, const QString &name,
                                 const QString &path, const QString &endpoint,
                                 const QString &apiModel, QString *errorMessage)
{
    QSqlQuery q(m_db);

    if (type == QStringLiteral("local")) {
        q.prepare(QStringLiteral("SELECT id FROM models WHERE type='local' AND path = :path"));
        q.bindValue(QStringLiteral(":path"), path);
    } else {
        q.prepare(QStringLiteral(
            "SELECT id FROM models WHERE type='external' AND endpoint = :ep AND api_model = :am"));
        q.bindValue(QStringLiteral(":ep"), endpoint);
        q.bindValue(QStringLiteral(":am"), apiModel);
    }

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return -1;
    }
    if (q.next()) {
        return q.value(0).toInt();
    }

    q.prepare(QStringLiteral(
        "INSERT INTO models (type, name, path, endpoint, api_model) "
        "VALUES (:type, :name, :path, :ep, :am)"));
    q.bindValue(QStringLiteral(":type"), type);
    q.bindValue(QStringLiteral(":name"), name);
    q.bindValue(QStringLiteral(":path"), textOrNull(path));
    q.bindValue(QStringLiteral(":ep"), textOrNull(endpoint));
    q.bindValue(QStringLiteral(":am"), textOrNull(apiModel));

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::insertHistory(const HistoryRecord &rec, QString *errorMessage)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO chat_history "
        "(model_id, prompt, answer, status, temperature, top_p, top_k, "
        " repeat_penalty, seed, max_tokens, context_size, threads, "
        " prompt_tokens, answer_tokens, duration_ms) "
        "VALUES (:mid, :prompt, :answer, :status, :temp, :topp, :topk, "
        " :rp, :seed, :maxtok, :ctx, :threads, :ptok, :atok, :dur)"));

    q.bindValue(QStringLiteral(":mid"), rec.modelId);
    q.bindValue(QStringLiteral(":prompt"), rec.prompt);
    q.bindValue(QStringLiteral(":answer"), rec.answer);
    q.bindValue(QStringLiteral(":status"),
                rec.status.isEmpty() ? QStringLiteral("success") : rec.status);
    q.bindValue(QStringLiteral(":temp"), rec.temperature);
    q.bindValue(QStringLiteral(":topp"), rec.topP);
    q.bindValue(QStringLiteral(":topk"), rec.topK);
    q.bindValue(QStringLiteral(":rp"), rec.repeatPenalty);
    q.bindValue(QStringLiteral(":seed"), rec.seed);
    q.bindValue(QStringLiteral(":maxtok"), rec.maxTokens);
    q.bindValue(QStringLiteral(":ctx"), rec.contextSize);
    q.bindValue(QStringLiteral(":threads"), rec.threads);
    q.bindValue(QStringLiteral(":ptok"), rec.promptTokens);
    q.bindValue(QStringLiteral(":atok"), rec.answerTokens);
    q.bindValue(QStringLiteral(":dur"), rec.durationMs);

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    return true;
}

QVector<HistoryRecord> DatabaseManager::queryHistory(const QString &searchText,
                                                     QString *errorMessage) const
{
    QVector<HistoryRecord> result;

    QString sql = QStringLiteral(
        "SELECT h.id, h.model_id, m.name, m.type, m.path, "
        "       h.prompt, h.answer, h.status, "
        "       h.temperature, h.top_p, h.top_k, h.repeat_penalty, h.seed, "
        "       h.max_tokens, h.context_size, h.threads, "
        "       h.prompt_tokens, h.answer_tokens, h.duration_ms, h.created_at "
        "FROM chat_history h "
        "LEFT JOIN models m ON h.model_id = m.id ");

    const QString keyword = searchText.trimmed();
    if (!keyword.isEmpty()) {
        sql += QStringLiteral(
            "WHERE h.prompt LIKE :kw OR h.answer LIKE :kw OR m.name LIKE :kw ");
    }
    sql += QStringLiteral("ORDER BY h.id ASC");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (!keyword.isEmpty()) {
        q.bindValue(QStringLiteral(":kw"), QStringLiteral("%%1%").arg(keyword));
    }

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return result;
    }

    while (q.next()) {
        HistoryRecord rec;
        int c = 0;
        rec.id            = q.value(c++).toInt();
        rec.modelId       = q.value(c++).toInt();
        rec.modelName     = q.value(c++).toString();
        rec.modelType     = q.value(c++).toString();
        rec.modelPath     = q.value(c++).toString();
        rec.prompt        = q.value(c++).toString();
        rec.answer        = q.value(c++).toString();
        rec.status        = q.value(c++).toString();
        rec.temperature   = q.value(c++).toDouble();
        rec.topP          = q.value(c++).toDouble();
        rec.topK          = q.value(c++).toInt();
        rec.repeatPenalty = q.value(c++).toDouble();
        rec.seed          = q.value(c++).toInt();
        rec.maxTokens     = q.value(c++).toInt();
        rec.contextSize   = q.value(c++).toInt();
        rec.threads       = q.value(c++).toInt();
        rec.promptTokens  = q.value(c++).toInt();
        rec.answerTokens  = q.value(c++).toInt();
        rec.durationMs    = q.value(c++).toInt();
        rec.createdAt     = q.value(c++).toString();
        result.append(rec);
    }
    return result;
}

bool DatabaseManager::getHistory(int id, HistoryRecord &out, QString *errorMessage) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT h.id, h.model_id, m.name, m.type, m.path, "
        "       h.prompt, h.answer, h.status, "
        "       h.temperature, h.top_p, h.top_k, h.repeat_penalty, h.seed, "
        "       h.max_tokens, h.context_size, h.threads, "
        "       h.prompt_tokens, h.answer_tokens, h.duration_ms, h.created_at "
        "FROM chat_history h "
        "LEFT JOIN models m ON h.model_id = m.id "
        "WHERE h.id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        if (errorMessage) *errorMessage = QStringLiteral("未找到该历史记录。");
        return false;
    }

    int c = 0;
    out.id            = q.value(c++).toInt();
    out.modelId       = q.value(c++).toInt();
    out.modelName     = q.value(c++).toString();
    out.modelType     = q.value(c++).toString();
    out.modelPath     = q.value(c++).toString();
    out.prompt        = q.value(c++).toString();
    out.answer        = q.value(c++).toString();
    out.status        = q.value(c++).toString();
    out.temperature   = q.value(c++).toDouble();
    out.topP          = q.value(c++).toDouble();
    out.topK          = q.value(c++).toInt();
    out.repeatPenalty = q.value(c++).toDouble();
    out.seed          = q.value(c++).toInt();
    out.maxTokens     = q.value(c++).toInt();
    out.contextSize   = q.value(c++).toInt();
    out.threads       = q.value(c++).toInt();
    out.promptTokens  = q.value(c++).toInt();
    out.answerTokens  = q.value(c++).toInt();
    out.durationMs    = q.value(c++).toInt();
    out.createdAt     = q.value(c++).toString();
    return true;
}

bool DatabaseManager::deleteHistory(int id, QString *errorMessage)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM chat_history WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::clearHistory(QString *errorMessage)
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("DELETE FROM chat_history"))) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    return true;
}