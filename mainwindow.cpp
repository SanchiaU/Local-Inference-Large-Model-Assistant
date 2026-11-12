#include "mainwindow.h"

#include "llamarunner.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include <QLineEdit>
#include <QMessageBox>
#include <QMetaMethod>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
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
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    auto *modeLayout = new QHBoxLayout();

    m_modelPathEdit = new QLineEdit(this);
    m_modelPathEdit->setPlaceholderText("请选择本地 GGUF 模型，例如：model/qwen2.5-0.5b-instruct-q4_k_m.gguff");

    auto *browseButton = new QPushButton("浏览模型",this);
    modeLayout->addWidget(new QLabel("模型",this));
    modeLayout->addWidget(m_modelPathEdit,1);
    modeLayout->addWidget(browseButton);

    auto *settingsBox = new QGroupBox("推理参数",this);
    auto *settingsLayout = new QFormLayout(settingsBox);

    //最大生成Token 数控制单次回答长度，默认256
    m_maxTokensSpin = new QSpinBox(this);
    m_maxTokensSpin->setRange(1,4096);
    m_maxTokensSpin->setValue(256);

    //上下文长度决定模型可利用的上下文窗口大小，步长 128
    m_contextSizeSpin = new QSpinBox(this);
    m_contextSizeSpin->setRange(128,32768);
    m_contextSizeSpin->setSingleStep(128);
    m_contextSizeSpin->setValue(2048);

    //CPU线程数默认取硬件并发能力
    m_threadsSpin =new QSpinBox(this);
    const int defaultThreads = std::max(1u,std::thread::hardware_concurrency());
    m_threadsSpin->setRange(1,128);
    m_threadsSpin->setValue(defaultThreads);

    //控制采样随机性
    m_temperatureSpin = new QDoubleSpinBox(this);
    m_temperatureSpin->setRange(0.0,2.0);
    m_temperatureSpin->setSingleStep(0.05);
    m_temperatureSpin->setDecimals(2);
    m_temperatureSpin->setValue(0.75);

    //Top-P
    m_topPSpin = new QDoubleSpinBox(this);
    m_topPSpin->setRange(0.0,1.0);
    m_topPSpin->setSingleStep(0.05);
    m_topPSpin->setValue(0.90);

    //Top-K
    m_topKSpin = new QSpinBox(this);
    m_topKSpin->setRange(1,10000);
    m_topKSpin->setValue(40);

    //重复惩罚用于降低模型持续重复相关词的概率
    m_repeatPenaltySpin = new QDoubleSpinBox(this);
    m_repeatPenaltySpin->setRange(1.0,2.0);
    m_repeatPenaltySpin->setSingleStep(0.05);
    m_repeatPenaltySpin->setDecimals(2);
    m_repeatPenaltySpin->setValue(1.15);

    //随机种子
    m_seedSpin = new QSpinBox(this);
    m_seedSpin->setRange(-1,2147483647);
    m_seedSpin->setValue(-1);
    m_seedSpin->setSpecialValueText("随机");

    settingsLayout->addRow("最大生成 Token",m_maxTokensSpin);
    settingsLayout->addRow("上下文长度",m_contextSizeSpin);
    settingsLayout->addRow("CPU线程数",m_threadsSpin);
    settingsLayout->addRow("Temperature",m_temperatureSpin);
    settingsLayout->addRow("Top-P",m_topPSpin);
    settingsLayout->addRow("Top-k",m_topKSpin);
    settingsLayout->addRow("重复惩罚",m_repeatPenaltySpin);
    settingsLayout->addRow("随机种子",m_seedSpin);

    //提示词
    m_promptEdit = new QTextEdit(this);
    m_promptEdit->setPlaceholderText("请输入提示词，例如：请用三句话介绍llama.cpp的作用");
    m_promptEdit->setMinimumHeight(120);

    //输出框
    m_outputEdit = new QTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setMinimumHeight(220);

    auto *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("开始生成",this);
    m_stopButton = new QPushButton("停止",this);
    m_stopButton->setEnabled(false);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);

    rootLayout->addLayout(modeLayout);
    rootLayout->addWidget(settingsBox);
    rootLayout->addWidget(new QLabel("提示词",this));
    rootLayout->addWidget(m_promptEdit);
    rootLayout->addWidget(new QLabel("输出",this));
    rootLayout->addWidget(m_outputEdit,1);
    rootLayout->addLayout(buttonLayout);

    setCentralWidget(central);

    //窗口 相关信息
    setWindowTitle("Qt + llama.cpp 本地推理工程实践");

    applyAdaptiveFixedWindowSize();

    connect(browseButton,&QPushButton::clicked,this,&MainWindow::chooseModel);
    connect(m_startButton,&QPushButton::clicked,this,&MainWindow::startGeneration);
    connect(m_stopButton,&QPushButton::clicked,this,&MainWindow::stopGeneration);
}

//析构主窗口并安全处理后台线程
MainWindow::~MainWindow(){
    stopGeneration();
    if(m_workerThread){
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void MainWindow::chooseModel(){
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择GGUF模型",
        QCoreApplication::applicationDirPath()+"/../../models",
        "GGUF Models(*.gguf);;ALL Files(*.*)");

    if(!filePath.isEmpty()){
        m_modelPathEdit->setText(filePath);
    }
}
/////////////////////////////////////////////////////
void MainWindow::startGeneration(){
    if(m_workerThread){
        return;
    }

    m_outputEdit->clear();
    setRunning(true);
    m_workerThread = new QThread(this);
    m_runner = new LlamaRunner();
    m_runner->moveToThread(m_workerThread);

    connect(m_workerThread,&QThread::finished,m_runner,&QObject::deleteLater);

    connect(m_workerThread,&QThread::finished,m_runner,&QObject::deleteLater);
    connect(m_runner,&LlamaRunner::tokenReady,this,&MainWindow::appendToken);
    connect(m_runner,&LlamaRunner::failed,this,&MainWindow::handleFailure);
    connect(m_runner,&LlamaRunner::finished,this,&MainWindow::handleFinished);

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

    connect(m_workerThread,&QThread::started,this,[=](){
        QMetaObject::invokeMethod(
            m_runner,
            "generate",
            Qt::QueuedConnection,
            Q_ARG(QString,modelPath),
            Q_ARG(QString,prompt),
            Q_ARG(int,maxTokens),
            Q_ARG(int,contextSize),
            Q_ARG(int,threads),
            Q_ARG(double,temperature),
            Q_ARG(double,topP),
            Q_ARG(int,topK),
            Q_ARG(double,repeatPenalty),
            Q_ARG(int,seed)
            );
    });
    m_workerThread->start();
}

void MainWindow::stopGeneration(){
    //为空表示没有可执行的后台操作
    if(!m_runner){
        return ;
    }

    m_stopButton->setEnabled(false);
    m_stopButton->setText(QStringLiteral("正在停止…"));

    m_runner->cancel();

}

void MainWindow::appendToken(const QString &token){
    m_outputEdit->moveCursor(QTextCursor::End);
    m_outputEdit->insertPlainText(token);
    m_outputEdit->moveCursor(QTextCursor::End);
}

void MainWindow::handleFinished(){
    if(m_workerThread){
        m_workerThread->quit();
        m_workerThread = nullptr;
        m_runner = nullptr;
    }
    m_runner = nullptr;               // 由 QThread::finished → deleteLater 回收
    m_stopButton->setText(QStringLiteral("停止"));
    setRunning(false);
}
//运行中失败
void MainWindow::handleFailure(const QString &message){
    QMessageBox::warning(this,"推理失败",message);
}

void MainWindow::setRunning(bool running){
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);

    //运行中锁定模型路径、提示词
    m_modelPathEdit->setEnabled(!running);
    m_promptEdit->setEnabled(!running);
    m_maxTokensSpin->setEnabled(!running);
    m_contextSizeSpin->setEnabled(!running);
    m_threadsSpin->setEnabled(!running);
    m_temperatureSpin->setEnabled(!running);
    m_topPSpin->setEnabled(!running);
    m_topKSpin->setEnabled(!running);
    m_repeatPenaltySpin->setEnabled(!running);
    m_seedSpin->setEnabled(!running);

}

void MainWindow::applyAdaptiveFixedWindowSize(){

    constexpr int baseWidth = 920;
    constexpr int baseHeight = 720;
    constexpr qreal maxScreenRatio = 0.85;

    const QScreen *screen = windowHandle()?windowHandle()->screen():QApplication::primaryScreen();

    if(!screen){
        setFixedSize(baseWidth, baseHeight);
        return;
    }

    const QSize availableSize = screen->availableGeometry().size();
    const qreal widthScale = static_cast<qreal>(availableSize.width())/baseWidth;
    const qreal heightScale = static_cast<qreal>(availableSize.height())/baseHeight;
    const qreal scale = std::min<qreal>(1.0,std::min(widthScale,heightScale)*maxScreenRatio);

    const QSize windowSize(
        std::max(1,qRound(baseWidth*scale)),
        std::max(1,qRound(baseHeight*scale))
    );

    setFixedSize(windowSize);



}