//
// Created by cline on 2/6/26.
//

#include "../include/configLoader.h"

#include <filesystem>

#include "../include/configStructs.h"

std::shared_ptr<Logger> configLoader::cLogger = nullptr;

configLoader::configLoader(std::string path) {
    toml_path = path;
}

bool configLoader::load(systemConfig *c) {
    if (std::filesystem::exists(toml_path)) {
        auto tbl = toml::parse_file(toml_path);

        return c->from_toml(tbl);
    }
    return false;
}

void configLoader::createDefaultCFGfile(systemConfig *c) {
    if (cLogger)
        cLogger->println(VerbosityLevel::INFO, SubsystemTag::CONFIG, "Creating default cfg file at location "+ toml_path);
    auto tbl = c->to_toml();

    std::ofstream out(toml_path);
    out<<tbl;
    out.close();
}

bool configLoader::validate(systemConfig *c) {
    return c->validate();
}
