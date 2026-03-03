#include "Utils.h"

void Utils::debug(ThreadName name, const QString &msg) {
    if (!DEBUG_ENABLED) return;

    // 获取枚举的字符串名称
    QMetaEnum metaEnum = QMetaEnum::fromType<Utils::ThreadName>();
    const char* threadStr = metaEnum.valueToKey(static_cast<int>(name));

    qDebug() << QString("[%1] %2").arg(threadStr).arg(msg);
}
