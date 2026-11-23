#include "mainwindow.h"

#include "llamarunner.h"
#include "deepseekclient.h"

#include <QApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaMethod>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <thread>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftPanel = new QWidget(splitter);
    auto *rootLayout = new QVBoxLayout(leftPanel);

    // ══════════ 第一行：模型来源切换 + API Key ══════════
    auto *apiLayout = new QHBoxLayout();

    m_localRadio = new QRadioButton(QStringLiteral("本地模型"), leftPanel);
    m_apiRadio = new QRadioButton(QStringLiteral("API"), leftPanel);
    m_localRadio->setChecked(true);                       // 默认本地模型模式

    m_apiKeyEdit = new QLineEdit(leftPanel);
    m_apiKeyEdit->setPlaceholderText(QStringLiteral("请输入sk-xxxxx"));
    m_apiConfirmButton = new QPushButton(QStringLiteral("确认"), leftPanel);

    apiLayout->addWidget(m_localRadio);
    apiLayout->addWidget(m_apiRadio);
    apiLayout->addWidget(m_apiKeyEdit, 1);
    apiLayout->addWidget(m_apiConfirmButton);

    // ══════════ 第二行：模型路径 ══════════
    auto *modeLayout = new QHBoxLayout();
    m_modelPathEdit = new QLineEdit(leftPanel);
    m_modelPathEdit->setPlaceholderText("请选择本地 GGUF 模型，例如：model/qwen2.5-0.5b-instruct-q4_k_m.gguff");
    m_browseButton = new QPushButton("浏览模型", leftPanel);
    modeLayout->addWidget(new QLabel("模型", leftPanel));
    modeLayout->addWidget(m_modelPathEdit, 1);
    modeLayout->addWidget(m_browseButton);

    // ══════════ 推理参数 ══════════
    m_settingsBox = new QGroupBox("推理参数", leftPanel);
    auto *settingsLayout = new QFormLayout(m_settingsBox);

    m_maxTokensSpin = new QSpinBox(leftPanel);
    m_maxTokensSpin->setRange(1, 4096);
    m_maxTokensSpin->setValue(256);

    m_contextSizeSpin = new QSpinBox(leftPanel);
    m_contextSizeSpin->setRange(128, 32768);
    m_contextSizeSpin->setSingleStep(128);
    m_contextSizeSpin->setValue(2048);

    m_threadsSpin = new QSpinBox(leftPanel);
    const int defaultThreads = std::max(1u, std::thread::hardware_concurrency());
    m_threadsSpin->setRange(1, 128);
    m_threadsSpin->setValue(defaultThreads);

    m_temperatureSpin = new QDoubleSpinBox(leftPanel);
    m_temperatureSpin->setRange(0.0, 2.0);
    m_temperatureSpin->setSingleStep(0.05);
    m_temperatureSpin->setDecimals(2);
    m_temperatureSpin->setValue(0.75);

    m_topPSpin = new QDoubleSpinBox(leftPanel);
    m_topPSpin->setRange(0.0, 1.0);
    m_topPSpin->setSingleStep(0.05);
    m_topPSpin->setValue(0.90);

    m_topKSpin = new QSpinBox(leftPanel);
    m_topKSpin->setRange(1, 10000);
    m_topKSpin->setValue(40);

    m_repeatPenaltySpin = new QDoubleSpinBox(leftPanel);
    m_repeatPenaltySpin->setRange(1.0, 2.0);
    m_repeatPenaltySpin->setSingleStep(0.05);
    m_repeatPenaltySpin->setDecimals(2);
    m_repeatPenaltySpin->setValue(1.15);

    m_seedSpin = new QSpinBox(leftPanel);
    m_seedSpin->setRange(-1, 2147483647);
    m_seedSpin->setValue(-1);
    m_seedSpin->setSpecialValueText("随机");

    settingsLayout->addRow("最大生成 Token", m_maxTokensSpin);
    settingsLayout->addRow("上下文长度", m_contextSizeSpin);
    settingsLayout->addRow("CPU线程数", m_threadsSpin);
    settingsLayout->addRow("Temperature", m_temperatureSpin);
    settingsLayout->addRow("Top-P", m_topPSpin);
    settingsLayout->addRow("Top-k", m_topKSpin);
    settingsLayout->addRow("重复惩罚", m_repeatPenaltySpin);
    settingsLayout->addRow("随机种子", m_seedSpin);

    // ══════════ 提示词 / 输出 ══════════
    m_promptEdit = new QTextEdit(leftPanel);
    m_promptEdit->setPlaceholderText("请输入提示词，例如：请用三句话介绍llama.cpp的作用");
    m_promptEdit->setMinimumHeight(120);

    m_outputEdit = new QTextEdit(leftPanel);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setMinimumHeight(220);

    auto *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("开始生成", this);
    m_stopButton = new QPushButton("停止", this);
    m_stopButton->setEnabled(false);
    m_clearButton = new QPushButton("清空", this);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_clearButton);

    rootLayout->addLayout(apiLayout);
    rootLayout->addLayout(modeLayout);
    rootLayout->addWidget(m_settingsBox);
    rootLayout->addWidget(new QLabel("提示词", leftPanel));
    rootLayout->addWidget(m_promptEdit);
    rootLayout->addWidget(new QLabel("输出", leftPanel));
    rootLayout->addWidget(m_outputEdit, 1);
    rootLayout->addLayout(buttonLayout);

    // ══════════ 右面板：历史问答侧边栏 ══════════
    auto *rightPanel = new QWidget(splitter);
    auto *sideLayout = new QVBoxLayout(rightPanel);

    m_searchEdit = new QLineEdit(rightPanel);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索历史记录…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_historyList = new QListWidget(rightPanel);
    m_historyList->setAlternatingRowColors(true);
    m_historyList->setMinimumWidth(220);

    m_refreshButton = new QPushButton(QStringLiteral("刷新"), rightPanel);
    m_clearHistoryButton = new QPushButton(QStringLiteral("清空历史"), rightPanel);

    auto *historyButtonLayout = new QHBoxLayout();
    historyButtonLayout->addWidget(m_refreshButton);
    historyButtonLayout->addWidget(m_clearHistoryButton);

    sideLayout->addWidget(new QLabel(QStringLiteral("历史问答"), rightPanel));
    sideLayout->addWidget(m_searchEdit);
    sideLayout->addWidget(m_historyList, 1);
    sideLayout->addLayout(historyButtonLayout);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({760, 260});

    setCentralWidget(splitter);

    setWindowTitle("deepseek + llama.cpp 双支持推理工程实践");
    applyAdaptiveFixedWindowSize();

    // ══════════ 信号连接 ══════════
    // 模式切换
    connect(m_localRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        m_apiMode = false;
        updateControlsEnabled();
    });
    connect(m_apiRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        m_apiMode = true;
        updateControlsEnabled();
    });

    // 点“确认” = 校验密钥、隐藏密钥、切到 API 模式
    connect(m_apiConfirmButton, &QPushButton::clicked, this, [this]() {
        const QString key = m_apiKeyEdit->text().trimmed();
        if (key.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("请先输入 API Key（sk-xxxxx）"));
            return;
        }
        m_apiKeyEdit->setEchoMode(QLineEdit::Password);   // ★ 隐藏密钥
        m_apiRadio->setChecked(true);                     // 切换到 API 模式
    });

    // 点“浏览模型” = 切回本地模型模式
    connect(m_browseButton, &QPushButton::clicked, this, [this]() {
        m_localRadio->setChecked(true);
        chooseModel();
    });

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startGeneration);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopGeneration);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearInputOutput);
    connect(m_clearHistoryButton, &QPushButton::clicked, this, &MainWindow::clearHistory);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshHistory);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshHistory);
    connect(m_historyList, &QListWidget::itemClicked, this, &MainWindow::onHistoryItemClicked);

    // ══════════ 初始化数据库 ══════════
    m_dbManager = new DatabaseManager();
    QString dbError;
    if (!m_dbManager->open(&dbError)) {
        QMessageBox::warning(this, QStringLiteral("数据库错误"),
                             QStringLiteral("无法打开历史数据库：\n%1").arg(dbError));
    }

    refreshHistory();
    updateControlsEnabled();   // 初始：本地模型模式，API 行禁用
    //=======dpsk类在构造函数里创建并连接===========
    m_deepSeekClient = new DeepSeekClient(this);

    connect(m_deepSeekClient, &DeepSeekClient::responseReceived,
            this, [this](const QString &content) {
                appendToken(content);   // 复用现有追加逻辑，同时累积 m_pendingAnswer
            });
    connect(m_deepSeekClient, &DeepSeekClient::requestFailed,
            this, &MainWindow::handleApiFailure);
    connect(m_deepSeekClient, &DeepSeekClient::finished,
            this, &MainWindow::handleApiFinished);
}

MainWindow::~MainWindow()
{
    stopGeneration();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    delete m_dbManager;
    m_dbManager = nullptr;
}

void MainWindow::chooseModel()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择GGUF模型",
        QCoreApplication::applicationDirPath() + "/../../models",
        "GGUF Models(*.gguf);;ALL Files(*.*)");

    if (!filePath.isEmpty()) {
        m_modelPathEdit->setText(filePath);
    }
}

void MainWindow::refreshHistory()
{
    m_records.clear();
    m_historyList->clear();

    if (!m_dbManager || !m_dbManager->isOpen()) {
        return;
    }

    QString error;
    const QVector<HistoryRecord> records =
        m_dbManager->queryHistory(m_searchEdit ? m_searchEdit->text() : QString(), &error);
    if (!error.isEmpty()) {
        qWarning() << "查询历史失败:" << error;
        return;
    }

    for (const HistoryRecord &rec : records) {
        appendHistoryItem(rec);
    }
    m_historyList->scrollToBottom();
}

void MainWindow::appendHistoryItem(const HistoryRecord &rec)
{
    m_records.append(rec);

    QString timePart = rec.createdAt;
    if (timePart.length() >= 16) {
        timePart = timePart.mid(5, 11);   // "2025-01-15 16:42:30" → "01-15 16:42"
    }

    QString question = rec.prompt.simplified();
    if (question.length() > 40) {
        question = question.left(40) + QStringLiteral("…");
    }
    if (question.isEmpty()) {
        question = QStringLiteral("(空提示词)");
    }

    const QString title = QStringLiteral("[%1] %2").arg(timePart, question);

    auto *item = new QListWidgetItem(title, m_historyList);
    item->setToolTip(QStringLiteral("时间：%1\n模型：%2\n\n%3")
                         .arg(rec.createdAt, rec.modelName, rec.prompt));
    item->setData(Qt::UserRole, m_records.size() - 1);
}

void MainWindow::onHistoryItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_records.size()) return;

    const HistoryRecord &rec = m_records.at(idx);
    m_promptEdit->setPlainText(rec.prompt);
    m_outputEdit->setPlainText(rec.answer);
}

// 按需求：只清空列表视图，不删除数据库记录
void MainWindow::clearHistory()
{
    m_records.clear();
    m_historyList->clear();
}

void MainWindow::startGeneration()
{
    if (m_workerThread) {
        return;
    }
    // API 模式：走 DeepSeek 网络请求
    if (m_apiMode) {
        startApiGeneration();
        return;
    }

    // // API 模式下暂不启动本地推理
    // if (m_apiMode) {
    //     QMessageBox::information(this, QStringLiteral("提示"),
    //                              QStringLiteral("当前为 API 模式，请切换到“本地模型”后再开始本地推理。"));
    //     return;
    // }

    m_outputEdit->clear();
    m_runFailed = false;
    m_runCancelled = false;
    m_pendingPrompt = m_promptEdit->toPlainText();
    m_pendingModelPath = m_modelPathEdit->text();
    m_pendingAnswer.clear();
    setRunning(true);

    m_workerThread = new QThread(this);
    m_runner = new LlamaRunner();
    m_runner->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::finished, m_runner, &QObject::deleteLater);
    connect(m_runner, &LlamaRunner::tokenReady, this, &MainWindow::appendToken);
    connect(m_runner, &LlamaRunner::failed, this, &MainWindow::handleFailure);
    connect(m_runner, &LlamaRunner::finished, this, &MainWindow::handleFinished);

    const QString modelPath = m_modelPathEdit->text();
    const QString prompt = m_promptEdit->toPlainText();
    const int maxTokens = m_maxTokensSpin->value();
    const int contextSize = m_contextSizeSpin->value();
    const int threads = m_threadsSpin->value();
    const double temperature = m_temperatureSpin->value();
    const double topP = m_topPSpin->value();
    const int topK = m_topKSpin->value();
    const double repeatPenalty = m_repeatPenaltySpin->value();
    const int seed = m_seedSpin->value();

    connect(m_workerThread, &QThread::started, this, [=]() {
        QMetaObject::invokeMethod(
            m_runner,
            "generate",
            Qt::QueuedConnection,
            Q_ARG(QString, modelPath),
            Q_ARG(QString, prompt),
            Q_ARG(int, maxTokens),
            Q_ARG(int, contextSize),
            Q_ARG(int, threads),
            Q_ARG(double, temperature),
            Q_ARG(double, topP),
            Q_ARG(int, topK),
            Q_ARG(double, repeatPenalty),
            Q_ARG(int, seed));
    });
    m_workerThread->start();
}

void MainWindow::stopGeneration()
{
    if (!m_running) {
        return;
    }

    m_runCancelled = true;
    m_stopButton->setEnabled(false);
    m_stopButton->setText(QStringLiteral("正在停止…"));

    if (m_deepSeekClient) {
        m_deepSeekClient->cancel();   // API 模式：中止网络请求
    }
    if (m_runner) {
        m_runner->cancel();           // 本地模式：取消 llama.cpp 推理
    }
}

void MainWindow::startApiGeneration()
{
    const QString apiKey = m_apiKeyEdit->text().trimmed();
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先输入 API Key（sk-xxxxx）"));
        return;
    }

    const QString prompt = m_promptEdit->toPlainText();
    if (prompt.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入提示词"));
        return;
    }

    m_outputEdit->clear();
    m_runFailed = false;
    m_runCancelled = false;
    m_pendingPrompt = prompt;
    m_pendingModelPath = QStringLiteral("deepseek-chat");
    m_pendingAnswer.clear();
    setRunning(true);

    m_deepSeekClient->setApiKey(apiKey);
    m_deepSeekClient->sendMessage(prompt,
                                  m_maxTokensSpin->value(),
                                  m_temperatureSpin->value());
}

void MainWindow::handleApiFinished()
{
    if (!m_runFailed && !m_runCancelled && !m_pendingAnswer.trimmed().isEmpty()
        && m_dbManager && m_dbManager->isOpen()) {

        QString error;
        const int modelId = m_dbManager->ensureModel(
            QStringLiteral("external"),
            QStringLiteral("deepseek-chat"),
            QString(),                                            // 外接模型 path 留空
            QStringLiteral("https://api.deepseek.com/chat/completions"),
            QStringLiteral("deepseek-chat"),
            &error);

        if (modelId > 0) {
            HistoryRecord rec;
            rec.modelId = modelId;
            rec.prompt = m_pendingPrompt;
            rec.answer = m_pendingAnswer;
            rec.status = QStringLiteral("success");
            rec.temperature = m_temperatureSpin->value();
            rec.topP = m_topPSpin->value();
            rec.topK = m_topKSpin->value();
            rec.repeatPenalty = m_repeatPenaltySpin->value();
            rec.seed = m_seedSpin->value();
            rec.maxTokens = m_maxTokensSpin->value();
            rec.contextSize = m_contextSizeSpin->value();
            rec.threads = m_threadsSpin->value();

            if (!m_dbManager->insertHistory(rec, &error)) {
                qWarning() << "插入历史失败:" << error;
            }
            refreshHistory();
        } else {
            qWarning() << "登记模型失败:" << error;
        }
    }

    m_stopButton->setText(QStringLiteral("停止"));
    setRunning(false);
}

void MainWindow::handleApiFailure(const QString &message)
{
    if (m_runCancelled) {
        return;   // 用户主动停止导致的 abort，不提示错误
    }
    m_runFailed = true;
    m_outputEdit->append(QStringLiteral("\n[API错误] %1").arg(message));
}

void MainWindow::clearInputOutput()
{
    m_promptEdit->clear();
    m_outputEdit->clear();
    m_promptEdit->setFocus();
}

void MainWindow::appendToken(const QString &token)
{
    m_pendingAnswer += token;
    m_outputEdit->moveCursor(QTextCursor::End);
    m_outputEdit->insertPlainText(token);
    m_outputEdit->moveCursor(QTextCursor::End);
}

void MainWindow::handleFinished()
{
    if (!m_runFailed && !m_runCancelled && !m_pendingAnswer.trimmed().isEmpty()
        && m_dbManager && m_dbManager->isOpen()) {

        const QFileInfo fi(m_pendingModelPath);

        QString error;
        const int modelId = m_dbManager->ensureModel(
            QStringLiteral("local"),
            fi.completeBaseName(),       // 去 .gguf：qwen2.5-1.5b-instruct-q4_k_m
            fi.absoluteFilePath(),
            QString(),
            QString(),
            &error);

        if (modelId > 0) {
            HistoryRecord rec;
            rec.modelId = modelId;
            rec.prompt = m_pendingPrompt;
            rec.answer = m_pendingAnswer;
            rec.status = QStringLiteral("success");
            rec.temperature = m_temperatureSpin->value();
            rec.topP = m_topPSpin->value();
            rec.topK = m_topKSpin->value();
            rec.repeatPenalty = m_repeatPenaltySpin->value();
            rec.seed = m_seedSpin->value();
            rec.maxTokens = m_maxTokensSpin->value();
            rec.contextSize = m_contextSizeSpin->value();
            rec.threads = m_threadsSpin->value();

            if (!m_dbManager->insertHistory(rec, &error)) {
                qWarning() << "插入历史失败:" << error;
            }
            refreshHistory();
        } else {
            qWarning() << "登记模型失败:" << error;
        }
    }

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    m_runner = nullptr;
    m_stopButton->setText(QStringLiteral("停止"));
    setRunning(false);
}

void MainWindow::handleFailure(const QString &message)
{
    m_runFailed = true;
    QMessageBox::warning(this, "推理失败", message);
}

void MainWindow::setRunning(bool running)
{
    m_running = running;
    updateControlsEnabled();
}

void MainWindow::setApiMode(bool apiMode)
{
    m_apiMode = apiMode;
    if (m_localRadio) m_localRadio->setChecked(!apiMode);
    if (m_apiRadio) m_apiRadio->setChecked(apiMode);
    updateControlsEnabled();
}

// 统一刷新所有控件可用状态：运行状态 × 模式状态
void MainWindow::updateControlsEnabled()
{
    const bool running = m_running;
    const bool api = m_apiMode;

    // 模式 radio：平时始终可点，运行中禁用
    m_localRadio->setEnabled(!running);
    m_apiRadio->setEnabled(!running);

    // API 行
    m_apiKeyEdit->setEnabled(api && !running);
    m_apiConfirmButton->setEnabled(api && !running);

    // 模型行
    m_modelPathEdit->setEnabled(!api && !running);
    m_browseButton->setEnabled(!api && !running);

    // 推理参数
    m_settingsBox->setEnabled(!api && !running);

    // 提示词 / 输出
    m_promptEdit->setEnabled(!running);
    m_outputEdit->setEnabled(!running);

    // 按钮
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_clearButton->setEnabled(!running);

    // 历史侧栏
    m_historyList->setEnabled(!running);
    m_searchEdit->setEnabled(!running);
    m_refreshButton->setEnabled(!running);
    m_clearHistoryButton->setEnabled(!running);
}

void MainWindow::applyAdaptiveFixedWindowSize()
{
    constexpr int baseWidth = 920;
    constexpr int baseHeight = 720;
    constexpr qreal maxScreenRatio = 0.85;

    const QScreen *screen = windowHandle() ? windowHandle()->screen() : QApplication::primaryScreen();

    if (!screen) {
        setFixedSize(baseWidth, baseHeight);
        return;
    }

    const QSize availableSize = screen->availableGeometry().size();
    const qreal widthScale = static_cast<qreal>(availableSize.width()) / baseWidth;
    const qreal heightScale = static_cast<qreal>(availableSize.height()) / baseHeight;
    const qreal scale = std::min<qreal>(1.0, std::min(widthScale, heightScale) * maxScreenRatio);

    const QSize windowSize(
        std::max(1, qRound(baseWidth * scale)),
        std::max(1, qRound(baseHeight * scale)));

    setFixedSize(windowSize);
}