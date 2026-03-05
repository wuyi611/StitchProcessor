#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QDebug>
#include <QObject>
#include <QMetaEnum>

#define DEBUG_ENABLED true // 建议改名，避免与系统宏冲突

class Utils {
    Q_GADGET // 非 QObject 派生类支持枚举注册的关键

public:
    // 1. 枚举必须定义在类内部，才能被 Q_ENUM 识别
    enum class ThreadName {
        DecoderThread,
        ProcessThread,
    };
    Q_ENUM(ThreadName) // 紧跟在枚举定义后面

    // 2. 必须指定返回类型 void，且参数不需要加 enum 关键字
    static void debug(ThreadName name, const QString &msg);
};

#endif // UTILS_H
