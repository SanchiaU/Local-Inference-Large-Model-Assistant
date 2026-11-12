#ifndef LLAMARUNNER_H
#define LLAMARUNNER_H

/*
@file llamarunner.h
@brief 本地 llama.cpp 文件生成工程对象声明文件
我声明 LlamaRunner类。该类封装了基于llama.cpp的本地GGUF模型文本生成任务接口。
作为业务类 后台工作对象移动到QThread中执行，避免阻塞Qt主界面线程

类功能：
接受模型路径、提示词和采样参数：
在实现文件中完成llama.cpp后端初始化、模型加载、上下文加载、采样器创建、
Prompt分词、Token逐步生成和资源释放；
通过Qt信号讲生成出的文本片段实时发送给界面层；
支持外部请求取消正在执行的生成任务；
在任务正确结束、取消结束或异常失败后向外部发送通知。

*/

#include <QObject>
#include <QString>
#include <atomic> //跨线程安全记录后台生成任务的取消状态

class LlamaRunner final: public QObject
{
    Q_OBJECT
public:
    explicit LlamaRunner(QObject *parent = nullptr);
public slots:
    void generate(QString modelPath,QString prompt,int maxTokens,int contextSize,int threads,double temperature,double topP,int topK,double repeatPenalty,int seed);
/*
    执行一次完整的本地文本生成任务
    步骤：1、校验模型路径和提示词
         2、初始化llama.cpp后端
         3、加载GGUF模型文件
         4、创建llama.cpp推理上下文
         5、根据采样参数创建采样器链
         6、对prompt进行分词写入上下文
         7、循环采样 Token、解码文本片段并发出tokenReady（）信号
         8、正常结束、取消结束或异常失败后释放相关资源并发出finished()信号
*/
    void cancel();//请求取消当前正在执行的生成任务
    void resetCancel();
signals:
    void tokenReady(QString token);//token：新生成并完成解码的文本片段，可直接追加显示到输出指定的控件当中
    void finished();//任务结束的时候，调用
    void failed(QString message);//当生成任务失败的时候发出 message:面向用户 或 调用方向错误
private:
    std::atomic_bool m_cancelled{false}; //生成任务取消标志


};

#endif // LLAMARUNNER_H
