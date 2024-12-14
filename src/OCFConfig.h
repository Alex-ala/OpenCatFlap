#ifndef __OCFCONFIG_H__
#define __OCFCONFIG_H__

#include <OCFHelper.h>
#include <ArxContainer.h>
#include <OCFCat.h>

struct OCFConfig{
    bool allow_in {false};
    bool allow_out {false};
};

class OCFState{
    public:
        inline static std::map<unsigned long long, OCFCat> cats;
        inline static OCFConfig config;
        static void saveCats();
        static void loadCats();
        static void loadConfig();
        static void saveConfig();
        static void getStateJSON(String& outStr);
        static void getCatsJSON(String& outStr);
};

#endif // __OCFCONFIG_H__