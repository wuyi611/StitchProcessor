// AppConfig.h
#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QSettings>
#include <QCoreApplication>
#include <QMutex>

class AppConfig {
public:
    // 获取单例实例
    static AppConfig& instance() {
        static AppConfig _instance;
        return _instance;
    }

    // 核心方法：从 ini 加载/刷新数据
    void load();

    // 核心方法：保存当前变量到 ini
    void save();

    // 全局变量定义
    QString videoPath;
    int threshold;
    bool isEnableGpu;

private:
    AppConfig() {
        // 设置 ini 文件路径为可执行程序所在目录的 config.ini
        m_iniPath = QCoreApplication::applicationDirPath() + "/config.ini";
        load(); // 初始化时自动读取一次
    }

    QString m_iniPath;
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;
};

#endif
