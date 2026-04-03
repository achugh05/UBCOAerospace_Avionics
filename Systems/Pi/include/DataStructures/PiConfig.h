//
// Created by cline on 3/26/26.
//

#ifndef GSE_PICONFIG_H
#define GSE_PICONFIG_H
#include "configStructs.h"
#include "../../../../Libraries/Video/include/Camera.h"


struct PiConfig : systemConfig {
    CameraConfig cameraConfig;

    bool validate() override{
        return systemConfig::validate()
            && cameraConfig.validate(&defaultValues);
    }

    bool from_toml(const toml::table& t) override{
        if (auto* nt = t["Camera"].as_table()) {
            cameraConfig.from_toml(*nt);
        } else
            return false;

        return systemConfig::from_toml(t);
    }

    toml::table to_toml() override {
        toml::table tbl = systemConfig::to_toml();

        tbl.insert("Camera", cameraConfig.to_toml());
        return tbl;
    }
};


#endif //GSE_PICONFIG_H