/*
主界面窗口

创建并组织Qt Widget用户界面空间
提供GGUF模型文件入口
收集提示词和推理参数
创建QThread与LlamaRunner后台工作对象，避免推理过程阻塞主界面线程
接受LlamaRunner发出的Token文本片段并追求显示
切换界面控件状态
显示窗口

多线程
MainWindow工作再QT GUI主线程
LlamaRunner被创建后移动到独立QThread中执行推理任务
MW通过QMetaObject::invokeMethod()请求LlamaRunner取消任务
*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QString>

#include "databasemanager.h"

class LlamaRunner;
class QLineEdit;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTextEdit;
class QThread;
class QListWidget;
class QListWidgetItem;
class DeepSeekClient;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void chooseModel();
    void startGeneration();
    void stopGeneration();
    void appendToken(const QString &token);
    void handleFinished();
    void handleFailure(const QString &message);
    void onHistoryItemClicked(QListWidgetItem *item);

    void clearHistory();
    void refreshHistory();
    void clearInputOutput();

    void startApiGeneration();
    void handleApiFinished();
    void handleApiFailure(const QString &message);

private:
    void setRunning(bool running);
    void setApiMode(bool apiMode);
    void updateControlsEnabled();
    void applyAdaptiveFixedWindowSize();
    void appendHistoryItem(const HistoryRecord &rec);

    // 模型来源切换 + API 输入
    QRadioButton *m_localRadio = nullptr;
    QRadioButton *m_apiRadio = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QPushButton *m_apiConfirmButton = nullptr;

    // 模型路径
    QLineEdit *m_modelPathEdit = nullptr;
    QPushButton *m_browseButton = nullptr;

    // 推理参数组
    QGroupBox *m_settingsBox = nullptr;

    QTextEdit *m_promptEdit = nullptr;
    QTextEdit *m_outputEdit = nullptr;

    QSpinBox *m_maxTokensSpin = nullptr;
    QSpinBox *m_contextSizeSpin = nullptr;
    QSpinBox *m_threadsSpin = nullptr;

    QDoubleSpinBox *m_temperatureSpin = nullptr;
    QDoubleSpinBox *m_topPSpin = nullptr;
    QSpinBox *m_topKSpin = nullptr;
    QDoubleSpinBox *m_repeatPenaltySpin = nullptr;
    QSpinBox *m_seedSpin = nullptr;

    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_clearButton = nullptr;

    QListWidget *m_historyList = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_clearHistoryButton = nullptr;

    QThread *m_workerThread = nullptr;
    LlamaRunner *m_runner = nullptr;

    DatabaseManager *m_dbManager = nullptr;

    QString m_pendingPrompt;
    QString m_pendingModelPath;
    QString m_pendingAnswer;
    bool m_runFailed = false;
    bool m_runCancelled = false;

    bool m_apiMode = false;   // true=API模式，false=本地模型模式
    bool m_running = false;   // 推理运行中

    QList<HistoryRecord> m_records;

    DeepSeekClient *m_deepSeekClient = nullptr;
};

#endif // MAINWINDOW_H