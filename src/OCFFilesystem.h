#ifndef __OCFFILESYSTEM_H__
#define __OCFFILESYSTEM_H__

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <definitions.h>

namespace OCFFilesystem{
    bool readJsonFile(const char* path, StaticJsonDocument<OCF_MAX_JSON_SIZE>& outRef);
    String readStringFile(const char * path);
    bool writeJsonFile(const char * path, DynamicJsonDocument document);
    void initSPIFFS();
};
#endif // __OCFFILESYSTEM_H__