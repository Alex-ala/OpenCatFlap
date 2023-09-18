#ifndef __OCFFILESYSTEM_H__
#define __OCFFILESYSTEM_H__

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <definitions.h>
// TODO: Use proper getter/setters or revert this to be non-classy
class OCFFilesystem{
    private:
        static OCFFilesystem filesystem;
        OCFFilesystem() {};
        OCFFilesystem(const OCFFilesystem&);
        OCFFilesystem operator=(const OCFFilesystem&);
    public:
        static bool readJsonFile(const char* path, StaticJsonDocument<OCF_MAX_JSON_SIZE>& outRef);
        static int readStringFile(const char * path, char* output, size_t max_size);
        static int getFileSize(const char* path);
        static bool writeStringFile(const char * path, const char* data);
        static bool writeJsonFile(const char * path, DynamicJsonDocument document);
        static void initSPIFFS();
};
#endif // __OCFFILESYSTEM_H__