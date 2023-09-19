#ifndef __OCFFILESYSTEM_H__
#define __OCFFILESYSTEM_H__

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <definitions.h>
class OCFFilesystem{
    private:
        static OCFFilesystem filesystem;
    public:
        static bool readJsonFile(const char* path, StaticJsonDocument<OCF_MAX_JSON_SIZE>& outRef);
        static int readStringFile(const char * path, char* output, size_t max_size);
        static int getFileSize(const char* path);
        static bool writeStringFile(const char * path, const char* data);
        static bool writeJsonFile(const char * path, DynamicJsonDocument document);
        static void initSPIFFS();
};
#endif // __OCFFILESYSTEM_H__