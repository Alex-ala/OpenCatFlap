#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>

#include <Arduino.h>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace mock
{
    inline std::map<std::string, std::string> files;
    inline std::set<std::string> directories;
    inline std::set<std::string> unopenable;
    inline bool mount_ok = true;
    inline int mount_calls = 0;
    inline int open_files = 0;

    inline void reset_fs()
    {
        files.clear();
        directories.clear();
        unopenable.clear();
        mount_ok = true;
        mount_calls = 0;
        open_files = 0;
    }
}

namespace fs
{
    class File : public Stream
    {
    public:
        File() {}

        static File open(const char *path, const char *mode)
        {
            File f;
            std::string p(path);
            if (mock::unopenable.count(p)) return f;
            bool directory = mock::directories.count(p) > 0;
            bool writing = mode[0] == 'w' || mode[0] == 'a';
            if (!directory && !writing && !mock::files.count(p)) return f;
            f.impl_ = std::make_shared<Impl>();
            f.impl_->path = p;
            f.impl_->directory = directory;
            f.impl_->writing = writing;
            if (mode[0] == 'w') mock::files[p].clear();
            if (!writing && !directory) f.impl_->data = mock::files[p];
            mock::open_files++;
            return f;
        }

        explicit operator bool() const { return impl_ && impl_->open; }
        bool isDirectory() const { return impl_ && impl_->directory; }
        size_t size() const
        {
            if (!impl_) return 0;
            return impl_->writing ? mock::files[impl_->path].size() : impl_->data.size();
        }
        void close()
        {
            if (impl_ && impl_->open)
            {
                impl_->open = false;
                mock::open_files--;
            }
        }

        size_t write(uint8_t c) override { return write(&c, 1); }
        size_t write(const uint8_t *buf, size_t size) override
        {
            if (!impl_ || !impl_->open || !impl_->writing) return 0;
            mock::files[impl_->path].append((const char *)buf, size);
            return size;
        }
        using Print::write;

        int available() override { return impl_ && !impl_->writing ? impl_->data.size() - impl_->pos : 0; }
        int read() override { return available() > 0 ? (uint8_t)impl_->data[impl_->pos++] : -1; }
        int peek() override { return available() > 0 ? (uint8_t)impl_->data[impl_->pos] : -1; }
        size_t read(uint8_t *buf, size_t size)
        {
            size_t n = 0;
            while (n < size && available() > 0) buf[n++] = (uint8_t)read();
            return n;
        }

    private:
        struct Impl
        {
            std::string path;
            std::string data;
            size_t pos = 0;
            bool directory = false;
            bool writing = false;
            bool open = true;
        };
        std::shared_ptr<Impl> impl_;
    };

    class FS
    {
    public:
        virtual ~FS() {}
        File open(const char *path, const char *mode = FILE_READ) { return File::open(path, mode); }
        File open(const String &path, const char *mode = FILE_READ) { return open(path.c_str(), mode); }
        bool exists(const char *path) { return mock::files.count(path) > 0; }
        bool remove(const char *path) { return mock::files.erase(path) > 0; }
    };
}

using fs::File;
using fs::FS;
