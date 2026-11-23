#include "llamarunner.h"

#include <QByteArray>
#include <QCoreApplication> //生成循环中处理Qt事件
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringDecoder>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "llama.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
    //将Qt文件路径转换为llama.cpp更容易的本地路径形式
    QString toLlamaReadablePath(const QString &path){

#ifdef Q_OS_WIN
        const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
        const DWORD required = GetShortPathNameW(nativePath.c_str(),nullptr,0);
        if(required>0){
            std::wstring shortPath(required,L'\0');
            const DWORD written = GetShortPathNameW(nativePath.c_str(),shortPath.data(),required);
            if(written>0 && written<required){  //去除未使用的尾部空间，只保留windows实际路径
                shortPath.resize(written);
                return QString::fromStdWString(shortPath);
            }
        }
#endif
        return path;
    }
}
/*
    @file llamarunner.cpp
    @brief 基于llama.cpp的本地GGUF模型文本生成工具对象实现文件
    LlamaRunner 是整个工程中负责连接Qt界面层与llama.cpp推理后端的后台工作对象。
    主要完成模型加载、上下文创建、采样器配置、prompt分词、Token自回归生成、文本片段解码、异常处理和资源释放、
*/

//构建llama.cpp推理工作对象
LlamaRunner::LlamaRunner(QObject *parent)
    : QObject{parent}
{

}

void LlamaRunner::cancel(){
    m_cancelled.store(true);
}

void LlamaRunner::resetCancel()  { m_cancelled.store(false); }

void LlamaRunner::generate(QString modelPath,
                           QString prompt,
                           int maxTokens,
                           int contextSize,
                           int threads,
                           double temperature,
                           double topP,
                           int topK,
                           double repeatPenalty,
                           int seed){
    //每次启动都要清除上次任务留下的取消标志，确保任务进入生成循环
    //
    //m_cancelled.store(false);
    if (m_cancelled.load()) {
        emit finished();
        return;
    }
    //
    //这个llama.cpp资源都是以裸指针形式返回的，所以需要手动释放
    //统一初始化为空指针，方便cleanup闭包判断哪些资源已经创建成功
    llama_model *model = nullptr;
    llama_context *ctx = nullptr;
    llama_sampler *sampler = nullptr;

    //定义变量单独记录 llama_backend_init()是否已经成功，避免异常路径中错误调用释放函数
    bool backendInitialized = false;

    //统一释放本次推荐过程中已经创建成功的 llama.cpp资源
    //先释放采样器、上下文、再释放模型，最后释放后端
    //释放置空,避免重复释放风险
    auto cleanup = [&](){
        if(sampler){
            llama_sampler_free(sampler);
            sampler = nullptr;
        }
        if(ctx){
            llama_free(ctx);
            ctx=nullptr;
        }
        if(model){
            llama_model_free(model);
            model = nullptr;
        }
        if(backendInitialized){
            llama_backend_free();
            backendInitialized = false;
        }
    };
    try{
        //模型路径为空，弹窗提示
        if(modelPath.trimmed().isEmpty()){
            throw std::runtime_error("请选择GGUF模型文件..>");
        }
        //Prompt为空，没有初始化语义输入，不启动推荐流程
        if(prompt.trimmed().isEmpty()){
            throw std::runtime_error("请输入提示词！");
        }
        //QFileInfo 查看文件存在和类型检查
        const QFileInfo modelFile(modelPath);
        if(!modelFile.exists()||!modelFile.isFile()){
            throw std::runtime_error("模型文件不存在,请重新选择GGUF文件");
        }
        llama_backend_init();
        backendInitialized = true;
        llama_model_params modelParams = llama_model_default_params();////////////////////////////////////////////////////////////////////////////////

        const QString readableModePath = toLlamaReadablePath(modelFile.absoluteFilePath());

        const QByteArray modelPathByte = QDir::toNativeSeparators(readableModePath).toLocal8Bit();
        model = llama_model_load_from_file(modelPathByte.constData(),modelParams);

        if(!model){
            const QString message = QString("模型加载失败。\n\n当前路径:%1\n路径传入路径:%2\n请确认文件路径是完整GGUF模型。若工程目录含中文仍加载失败，请开启Window 8.3短文件名或使用纯英文目录。").arg(modelPath,readableModePath);
            throw std::runtime_error(message.toUtf8().constData());
        }
        //上下文参数决定推理时的上下文窗口容器和CPU并行度
        llama_context_params ctxParams = llama_context_default_params();

        ctxParams.n_ctx = static_cast<uint32_t>(std::max(contextSize,128));  //至少128

        //n_threads 和 n_threads_batch 至少为1，用于普通解码和批处理阶段
        ctxParams.n_threads = std::max(threads,1);
        ctxParams.n_threads_batch = std::max(threads,1);

        ctx = llama_init_from_model(model,ctxParams);
        if(!ctx){
            throw std::runtime_error("上下文创建失败，可能是内存不足或参数不兼容。");
        }


        llama_sampler_chain_params samplerParams = llama_sampler_chain_default_params();
        sampler = llama_sampler_chain_init(samplerParams);
        if(!sampler){
            throw std::runtime_error("采样器创建失败。");
        }
        //对界面传入的采样参数做边界保护，防止非法值输入 llama.cpp采样器
        const int safeTopK = std::max(topK,1);
        const float safeTopP = static_cast<float>(std::clamp(topP,0.0,1.0));
        const float safeTemperature = static_cast<float>(std::clamp(temperature,0.0,1.0));
        const float safeRepeatPenalty = static_cast<float>(std::max(repeatPenalty,1.0));


        const uint32_t safeSeed = seed<0?LLAMA_DEFAULT_SEED:static_cast<uint32_t>(seed);

        //采样器链按顺序对候选 Token 分布进行top-k top-p 重复惩罚温度缩放 和随机采样
        llama_sampler_chain_add(sampler,llama_sampler_init_top_k(safeTopK));
        llama_sampler_chain_add(sampler,llama_sampler_init_top_p(safeTopP,1));
        llama_sampler_chain_add(sampler,llama_sampler_init_penalties(128,safeRepeatPenalty,0.04f,0.10f));
        llama_sampler_chain_add(sampler,llama_sampler_init_temp(safeTemperature));
        llama_sampler_chain_add(sampler,llama_sampler_init_dist(safeSeed));

        //从模型中取得词表对象，后续分词、判断结束token和token转文本都依赖词表
        const llama_vocab *vocab = llama_model_get_vocab(model);//改BUG后被提前了->147->解决了167行5哥参数的BUG
        ///
        // ── 1. 取模型内嵌模板
        const char * tmpl = llama_model_chat_template(model, nullptr);
        if (!tmpl) {
            throw std::runtime_error("模型内没有 chat template");
        }

        // ── 2. 按 OpenAI 的 role/content 组织（llama_chat_message 就是这个结构）
        QByteArray roleSys = "system", contSys = "You are a helpful assistant.";
        QByteArray roleUsr = "user",   contUsr = prompt.toUtf8();

        const llama_chat_message chat[2] = {
                                             { roleSys.constData(), contSys.constData() },
                                             { roleUsr.constData(), contUsr.constData() },
                                             };

        // ── 3. 应用模板（add_ass = true 是关键！）
        std::vector<char> buf(static_cast<size_t>(ctxParams.n_ctx) * 8 + 1);
        int32_t len = llama_chat_apply_template(
            tmpl, chat, 2,
            /* add_ass = */ true,          // ★ 末尾补 "<|im_start|>assistant\n"
            buf.data(), static_cast<int32_t>(buf.size()));

        if (len < 0) {
            throw std::runtime_error("chat template 应用失败");
        }
        if (len > static_cast<int32_t>(buf.size())) {   // 缓冲不够，按返回值扩容重来
            buf.resize(static_cast<size_t>(len) + 1);
            len = llama_chat_apply_template(tmpl, chat, 2, true,
                                            buf.data(), static_cast<int32_t>(buf.size()));
            if (len < 0 || len > static_cast<int32_t>(buf.size())) {
                throw std::runtime_error("chat template 缓冲区分配失败");
            }
        }

        const QByteArray promptBytes(buf.data(), len);

        // ── 4. 分词：add_special 必须改成 false
        const int32_t probe = llama_tokenize(
            vocab, promptBytes.constData(), static_cast<int32_t>(promptBytes.size()),
            nullptr, 0,
            /* add_special = */ false,   // ★ 模板已含特殊 token，再加就重复了
            /* parse_special = */ true);
        ///
        //
        /*
        const QByteArray promptBytes = prompt.toUtf8();

        // ── 第一次：探测所需 token 数
        //    缓冲区不足时返回“所需数量的相反数”，所以要取负
        const int32_t probe = llama_tokenize(
            vocab,
            promptBytes.constData(),
            static_cast<int32_t>(promptBytes.size()),
            nullptr,
            0,
            true,
            true);
        */
        if (probe >= 0) {
            // 非空 prompt 正常应返回负数；返回 0 说明分词结果为空
            throw std::runtime_error("提示词分词失败：未获得有效 token 数。");
        }

        const int32_t promptTokenCount = -probe;   // 负 → 正
        std::vector<llama_token> promptTokens(static_cast<size_t>(promptTokenCount));

        // ── 第二次：真正写入。成功返回正数，绝对值不够时返回负数
        const int32_t actualTokenCount = llama_tokenize(
            vocab,
            promptBytes.constData(),
            static_cast<int32_t>(promptBytes.size()),
            promptTokens.data(),
            static_cast<int32_t>(promptTokens.size()),
            false,//
            true);

        if (actualTokenCount < 0) {
            // 现在这个分支才是真正的“缓冲区不足”，并给出还差多少
            const QString msg = QString("提示词分词缓冲区不足，还差 %1 个 token。").arg(-actualTokenCount);
            throw std::runtime_error(msg.toUtf8().constData());
        }

        promptTokens.resize(static_cast<size_t>(actualTokenCount));

        //将完整的propmpt解码进入上下文，使模型获取初始化语义信息建立 kv cache
        llama_batch batch = llama_batch_get_one(promptTokens.data(),static_cast<int32_t>(promptTokens.size()));
        if(llama_decode(ctx,batch)!=0){
            throw std::runtime_error("提示词解码失败！");
        }

        QStringDecoder decoder(QStringDecoder::Utf8);

        int generated = 0;

        //保证满足取消标志和最大token数限制
        while(!m_cancelled.load() && generated<maxTokens){
            llama_token newTokenId = llama_sampler_sample(sampler,ctx,-1);

            llama_sampler_accept(sampler,newTokenId);

            // ── 临时诊断：前 30 个 token 的 id 和原文
            if (generated < 30) {
                char dbg[256];
                const int n = llama_token_to_piece(vocab, newTokenId, dbg, sizeof(dbg), 0, true);
                qDebug() << "tok" << generated << "id =" << newTokenId
                         << QString::fromUtf8(dbg, n > 0 ? n : 0);
            }

            if(llama_vocab_is_eog(vocab,newTokenId)){
                qDebug() << "EOG hit at" << generated;
                break;
            }
            char tokenBuffer[256];
            const int tokenLength = llama_token_to_piece(
                vocab,
                newTokenId,
                tokenBuffer,
                sizeof(tokenBuffer),
                0,
                true
            );

            if(tokenLength>0){
                const QString tokenText = decoder(QByteArrayView(tokenBuffer,tokenLength));
                if(!tokenText.isEmpty()){
                    emit tokenReady(tokenText);
                }
            }

            llama_batch nextBatch = llama_batch_get_one(&newTokenId,1);
            if(llama_decode(ctx,nextBatch)!=0){
                throw std::runtime_error("生成过程中解码失败，可能超过上下文长度。");
            }

            ++generated;
            QCoreApplication::processEvents();
        }

        const QString tail = decoder(QByteArrayView());
        if(!tail.isEmpty()){
            emit tokenReady(tail);
        }
        cleanup();
        emit finished();

    }catch(const std::exception &error){
        cleanup();

        emit failed(QString::fromUtf8(error.what()));

        emit finished();
    }


}

//void LlamaRunner::tokenReady(QString token){}

//void LlamaRunner::finished(){}

//void LlamaRunner::failed(QString message){}

