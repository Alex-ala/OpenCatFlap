#ifndef __OCFFILESYSTEM_H__
#define __OCFFILESYSTEM_H__

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <definitions.h>

namespace OCFFilesystem{
    bool readJsonFile(const char* path, StaticJsonDocument<OCF_MAX_JSON_SIZE>& outRef);
    int readStringFile(const char * path, char* output, size_t max_size);
    int getFileSize(const char* path);
    bool writeStringFile(const char * path, const char* data);
    bool writeJsonFile(const char * path, DynamicJsonDocument document);
    void initSPIFFS();
};
#endif // __OCFFILESYSTEM_H__