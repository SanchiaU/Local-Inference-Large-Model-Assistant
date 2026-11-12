QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    llamarunner.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    llamarunner.h \
    mainwindow.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    images.qrc

#配置如下：
DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN
LLAMA_CPP_DIR = $$PWD/external/llama.cpp

!exists($$LLAMA_CPP_DIR/include/llama.h){
    error("Cannot find llama.h Please put llama.cpp into external/llama.cpp or modify LLAMA_CPP_DIR in LocalLlamacpp.pro.")
}

#全局包含路径（头文件搜索路径）
#INCLUDEPATH:添加目录后，可以直接写#include"llama.cpp"
INCLUDEPATH +=\
    $$LLAMA_CPP_DIR/include\
    $$LLAMA_CPP_DIR/common\
    $$LLAMA_CPP_DIR/ggml/include\
    $$LLAMA_CPP_DIR/ggml

#win32-g++ :该条件模块要在目标平台为Windows且编译器为MinGW的时候生效
win32-g++{
    LLAMA_LIB_DIR =  D:/repository/5qt/qtreview/qt6/LocalLlamacpp/llama_qt_libs
    !exists($$LLAMA_LIB_DIR/libllama.a){
        error("Cannot find D:/repository/5qt/qtreview/qt6/LocalLlamacpp/llama_qt_libs/libllama.a. Please copy llama.cpp MinGW static libraries to D:/repository/5qt/qtreview/qt6/LocalLlamacpp/llama_qt_libs.")
    }

    LIBS+=\
    $$LLAMA_LIB_DIR/libllama.a\    #llama.app核心库
    $$LLAMA_LIB_DIR/ggml.a\        #ggml张量库
    $$LLAMA_LIB_DIR/ggml-cpu.a\    #ggml cpu后端
    $$LLAMA_LIB_DIR/ggml-base.a\   #ggml基础组件
    -fopenmp\                      #启用OPenMP支持（并行计算）
    -lstdc++\                      #链接标准C++库
    -lws2_32\                      #Windows Sockets2库
    -luser32\                      #Windows用户接口库（窗口、消息等）
    -lkernel32                     #Windows内核库（基本系统调用）
}
unix{  #用于Linux macOS
#库文件直接指向之前构建llama.cpp生成的Build目录
    LLAMA_LIB_DIR=$$LLAMA_CPP_DIR/BUILDS
    LIBS+=\
    $$LLAMA_CPP_DIR/libllama.a\
    $$LLAMA_CPP_DIR/ggml.a\
    $$LLAMA_CPP_DIR/ggml-cpu.a\
    $$LLAMA_CPP_DIR/ggml-base.a\
    -lpthread\                     #POSIX线程库.  llama.cpp依赖多线程
    -ldl                           #动态链接加载库，用于运行的时候加载动态库

}

