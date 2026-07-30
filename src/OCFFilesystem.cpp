#include <OCFFilesystem.h>
#include <memory>

bool mounted = false;

void OCFFilesystem::initSPIFFS()
{
    if (!SPIFFS.begin(false))
    {
        log_d("An error has occured while mounting SPIFFS");
    }
    else
    {
        mounted = true;
        log_d("SPIFFS mounted successfully");
    }
}
bool OCFFilesystem::readJsonFile(const char *path, StaticJsonDocument<OCF_MAX_JSON_SIZE> &outRef)
{
    log_d("Reading %s...", path);
    fs::FS &fs = SPIFFS;
    File file = fs.open(path, FILE_READ);
    if (!file || file.isDirectory())
    {
        log_d("Failed to open %s", path);
        return false;
    }
    DeserializationError err = deserializeJson(outRef, file);
    file.close();
    if (err)
    {
        log_d("Failed to parse %s: %s", path, err.c_str());
        return false;
    }
    log_d("Successfully read %s", path);
    return true;
}

bool OCFFilesystem::writeJsonFile(const char *path, DynamicJsonDocument document)
{
    if (!mounted)
        initSPIFFS();
    log_d("About to write to %s", path);
    fs::FS &fs = SPIFFS;
    File file = fs.open(path, FILE_WRITE);
    if (!file || file.isDirectory())
    {
        log_d("Failed to open %s", path);
        return false;
    }
    size_t written = serializeJson(document, file);
    file.close();
    if (written > 0)
    {
        log_d("Successfully wrote to %s", path);
        return true;
    }
    log_d("Failed to write %s", path);
    return false;
}

bool OCFFilesystem::writeJsonFile(const char *path, StaticJsonDocument<OCF_MAX_JSON_SIZE> document)
{
    if (!mounted)
        initSPIFFS();
    log_d("About to write to %s", path);
    fs::FS &fs = SPIFFS;
    File file = fs.open(path, FILE_WRITE);
    if (!file || file.isDirectory())
    {
        log_d("Failed to open %s", path);
        return false;
    }
    size_t written = serializeJson(document, file);
    file.close();
    if (written > 0)
    {
        log_d("Successfully wrote to %s", path);
        return true;
    }
    log_d("Failed to write %s", path);
    return false;
}

int OCFFilesystem::getFileSize(const char *path)
{
    fs::FS &fs = SPIFFS;
    File file = fs.open(path, FILE_READ);
    if (!file || file.isDirectory())
    {
        log_d("Failed to open %s", path);
        return 0;
    }
    int size = file.size();
    file.close();
    return size;
}

String OCFFilesystem::readStringFile(const char *path)
{
    fs::FS &fs = SPIFFS;
    File file = fs.open(path, FILE_READ);
    if (!file || file.isDirectory())
    {
        log_d("Failed to open %s", path);
        return String();
    }
    size_t size = file.size();
    if (size > OCF_MAX_FILE_READ_SIZE)
    {
        log_e("%s is %u bytes, refusing to read more than %u", path, (unsigned)size, (unsigned)OCF_MAX_FILE_READ_SIZE);
        file.close();
        return String();
    }
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[size + 1]);
    if (!buffer)
    {
        log_e("Out of memory reading %s (%u bytes)", path, (unsigned)size);
        file.close();
        return String();
    }
    size_t bytesRead = file.read((uint8_t *)buffer.get(), size);
    file.close();
    buffer[bytesRead] = '\0';
    return String(buffer.get());
}

bool OCFFilesystem::writeStringFile(const char *path, const char *data)
{
    if (!mounted)
        initSPIFFS();
    log_d("About to write to %s", path);
    fs::FS &fs = SPIFFS;
    File file = fs.open(path, FILE_WRITE);
    if (!file || file.isDirectory())
    {
        log_d("Failed to open %s", path);
        return false;
    }
    size_t len = strlen(data);
    size_t written = file.write((const uint8_t *)data, len);
    file.close();
    if (written != len)
    {
        log_d("Wrote only %u of %u bytes to %s", (unsigned)written, (unsigned)len, path);
        return false;
    }
    return true;
}
