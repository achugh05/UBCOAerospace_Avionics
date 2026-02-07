//
// Created by cline on 2/6/26.
//

#ifndef GSE_CONFIGLOADER_H
#define GSE_CONFIGLOADER_H
#include <toml++/toml.hpp>
#include <Logger.h>

struct systemConfig;

class Logger;

class configLoader {
    std::string toml_path;
    static std::shared_ptr<Logger> cLogger;


public:
    explicit configLoader(std::string path);
    bool load(systemConfig* c);
    void createDefaultCFGfile(systemConfig* c);
    bool validate(systemConfig* c);

    static void setLogger(std::shared_ptr<Logger> logger){cLogger = logger;};
    static std::shared_ptr<Logger> getLogger(){return cLogger;}
};


#endif //GSE_CONFIGLOADER_H