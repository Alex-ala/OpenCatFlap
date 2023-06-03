#include <OCFFilesystem.h>

namespace OCFFilesystem{
    void initSPIFFS(){
        if (!SPIFFS.begin(true)) {
            log_d("An error has occured while mounting SPIFFS");
        }
        log_d("SPIFFS mounted successfully");
    }
    bool readJsonFile(const char* path, StaticJsonDocument<OCF_MAX_JSON_SIZE>& outRef){
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

    String readStringFile(const char * path){
        fs::FS &fs = SPIFFS;
        File file = fs.open(path, FILE_READ);
        if(!file || file.isDirectory()){
            log_d("Failed to open %s", path);
            return String();
        }
        String fileContent;
        while(file.available()){
            fileContent = file.readStringUntil('\0');
            break;     
        }
        return fileContent;
    }
}