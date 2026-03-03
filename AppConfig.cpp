#include "AppConfig.h"


void AppConfig::load() {
    QSettings settings(m_iniPath, QSettings::IniFormat);

    // 读取配置，第二个参数是默认值（防止 ini 不存在时程序崩溃）
    videoPath = settings.value("Video/Path", "0").toString();
    threshold = settings.value("Algorithm/Threshold", 128).toInt();
    isEnableGpu = settings.value("System/UseGPU", false).toBool();
}

void AppConfig::save() {
    QSettings settings(m_iniPath, QSettings::IniFormat);
    settings.setValue("Video/Path", videoPath);
    settings.setValue("Algorithm/Threshold", threshold);
    settings.setValue("System/UseGPU", isEnableGpu);
}
