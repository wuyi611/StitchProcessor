#ifndef APPCONFIG_H
#define APPCONFIG_H

#if defined(DEBUG) || defined(_DEBUG)
#define BasePt "/../../../"
#else
#define BasePt "/"
#endif

#define CfgPt  "config.ini"

class AppConfig
{
public:
    static AppConfig& getInstance() {
        static AppConfig instance;
        return instance;
    }

    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;


    static constexpr int MAX_VIDEO_CNT = 10;
    static constexpr int MAX_QUEUE_SIZE = 3;

    int width;
    int height;


private:
    AppConfig();
    ~AppConfig() = default;

};



#endif // APPCONFIG_H
