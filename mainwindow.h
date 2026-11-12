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

#include <QMainWindow>

#include <QString>

class LlamaRunner;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QThread;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private slots:
    void chooseModel();//打开文件选择对话框并选择GGUF模型文件
    void startGeneration();//启动一次本地文本生成任务
    void stopGeneration();
    void appendToken(const QString &token);
    void handleFinished();//处理后台生成任务结束事件
    void handleFailure(const QString &message);
private:
    void setRunning(bool running);  //界面运行状态
    void applyAdaptiveFixedWindowSize();

    QLineEdit *m_modelPathEdit = nullptr;
    QTextEdit *m_promptEdit = nullptr;
    QTextEdit *m_outputEdit = nullptr;

    QSpinBox *m_maxTokensSpin = nullptr;
    QSpinBox *m_contextSizeSpin = nullptr;          //上下文长度
    QSpinBox *m_threadsSpin = nullptr;              //CPU推理线程数控件

    QDoubleSpinBox *m_temperatureSpin = nullptr;
    QDoubleSpinBox *m_topPSpin = nullptr;           //核采样
    QSpinBox *m_topKSpin = nullptr;
    QDoubleSpinBox *m_repeatPenaltySpin = nullptr;  //重复惩罚
    QSpinBox *m_seedSpin = nullptr;

    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;

    QThread *m_workerThread = nullptr;              //后台推理工作线程指针，仅在生成任务运行期间有效
    LlamaRunner *m_runner = nullptr;                //后台llamam.cpp推理工作对象指针，仅在生成任务运行期间有效





};
#endif // MAINWINDOW_H
