#include <OCFFilesystem.h>

namespace OCFFilesystem{
    void initSPIFFS(){
        if (!SPIFFS.begin(true)) {
            log_d("An error has occured while mounting SPIFFS");
        }
        log_d("SPIFFS mounted successfully");
    }
    bool readJsonFile(const char* path, StaticJsonDocument<OCF_MAX_JSON_SIZE>& outRef){
        log_d("Reading %s...", path);
        fs::FS &fs = SPIFFS;
        File file = fs.open(path, FILE_READ);
        if(!file || file.isDirectory()){
            log_d("Failed to open %s", path);
            return false;
        }
        DeserializationError err = deserializeJson(outRef, file);
        file.close();
        if (err){
            log_d("Failed to parse %s: %s", path, err.c_str());
            return false;
        }
        log_d("Successfully read %s", path);
        return true;
    }

    bool writeJsonFile(const char * path, DynamicJsonDocument document){
        log_d("About to write to %s", path);
        fs::FS &fs = SPIFFS;
        File file = fs.open(path, FILE_WRITE);
        if(!file || file.isDirectory()){
            log_d("Failed to open %s", path);
            return false;
        }
        if(serializeJson(document, file) > 0){
            log_d("Successfully wrote to %s", path);
            return true;
        }
        log_d("Failed to write %s", path);
        return false;    
    }

    int getFileSize(const char * path){
        fs::FS &fs = SPIFFS;
        File file = fs.open(path, FILE_READ);
        if(!file || file.isDirectory()){
            log_d("Failed to open %s", path);
            return 0;
        }
        return file.available();
    }

    int readStringFile(const char * path, char* output, size_t max_size){
        fs::FS &fs = SPIFFS;
        File file = fs.open(path, FILE_READ);
        if(!file || file.isDirectory()){
            log_d("Failed to open %s", path);
            return 0;
        }
        size_t bytesRead = 0;
        bytesRead = file.readBytesUntil('\0', output, max_size);
        output[bytesRead] = '\0';
        file.close();
        return bytesRead;
    }

    bool writeStringFile(const char * path, const char* data){
        log_d("About to write to %s", path);
        fs::FS &fs = SPIFFS;
        File file = fs.open(path, FILE_WRITE);
        if(!file || file.isDirectory()){
            log_d("Failed to open %s", path);
            return false;
        }
        file.printf("%s\0", data);
        file.close();
        return true;
    }
}