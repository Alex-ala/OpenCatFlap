#include <OCFConfig.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFCat.h>
#include <OCFFlapControl.h>

void OCFState::loadConfig(){
    Guard guard;
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    bool loaded = OCFFilesystem::readJsonFile(FLAP_CONFIG_FILE, doc);
    if (!loaded){
        log_d("Failed to load flap config, using defaults");
        config.allow_in = false;
        config.allow_out = false;
    }
    if (doc.containsKey("allowed_in")) config.allow_in = doc["allowed_in"].as<bool>();
    if (doc.containsKey("allowed_out")) config.allow_out = doc["allowed_out"].as<bool>();
    doc.clear();
    log_d("Flap config loaded. In: %d, Out: %d", config.allow_in, config.allow_out);
}

void OCFState::saveConfig() {
    Guard guard;
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["allowed_in"] = config.allow_in;
    doc["allowed_out"] = config.allow_out;
    OCFFilesystem::writeJsonFile(FLAP_CONFIG_FILE,doc);
    doc.clear();
}

void OCFState::loadCats(){
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    bool loaded = OCFFilesystem::readJsonFile(CAT_CONFIG_FILE, doc);
    if (!loaded){
        log_d("Failed to load cat config, no cats known");
        return;
    }
    Guard guard;
    log_d("Loading cats...");
    for (JsonObject elem : doc.as<JsonArray>()){
        OCFCat cat = OCFCat();
        char out[500];
        serializeJson(elem,out);
        log_d("elem: %s", out );
        copyString(cat.name, sizeof(cat.name), elem["name"].as<const char*>());
        if (elem.containsKey("allowed_in")) cat.allow_in = elem["allowed_in"].as<bool>();
        if (elem.containsKey("allowed_out")) cat.allow_out = elem["allowed_out"].as<bool>();
        if (elem.containsKey("last_seen")) {
            cat.last_seen = elem["last_seen"].as<uint64_t>();
        } else {
            cat.last_seen = 0;
        }
        if (elem.containsKey("location")) {
            cat.location = static_cast<OCFDirection>(elem["location"].as<int>());
        } else {
            cat.location = OCFDirection::NONE;
        }
        cat.rfid = elem["rfid"].as<unsigned long long>();
        cats.insert_or_assign(cat.rfid, cat);
        log_d("Loaded cat %s", cat.name);
     }
}

void OCFState::saveCats() {
    Guard guard;
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& m: cats){
        OCFCat cat = m.second;
        JsonObject obj = arr.createNestedObject();
        obj["name"] = cat.name;
        obj["rfid"] = cat.rfid;
        obj["allowed_in"] = cat.allow_in;
        obj["allowed_out"] = cat.allow_out;
        obj["last_seen"] = cat.last_seen;
        obj["location"] = cat.location;
    }
    OCFFilesystem::writeJsonFile(CAT_CONFIG_FILE,doc);
    doc.clear();
}

static void addReader(JsonObject obj, OCFRFIDFrequency frequency, OCFRFIDStats stats){
    obj["hz"] = frequency.hz;
    if (frequency.hz) obj["age_ms"] = frequency.age_ms;
    obj["edges"] = stats.edges;
    obj["glitches"] = stats.glitches;
    obj["headers"] = stats.headers;
    obj["frames"] = stats.frames;
    obj["repaired"] = stats.repaired;
    obj["crc_errors"] = stats.crc_errors;
    String hist;
    for (int i = 0; i < OCF_RFID_HIST_BINS; i++) {
        if (i) hist += ',';
        hist += stats.hist[i];
    }
    obj["hist"] = hist;
}

void OCFState::getStateJSON(String& outStr){
    Guard guard;
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["allow_out"] = config.allow_out;
    doc["allow_in"] = config.allow_in;
    doc["outward_state"] = OCFFlapControl::getOutwardState();
    doc["inward_state"] = OCFFlapControl::getInwardState();
    doc["mem_free_heap"] = esp_get_free_heap_size();
    doc["time"] = OCFWifi::getEpochTime();
    JsonObject rfid = doc["rfid"].to<JsonObject>();
    addReader(rfid["inside"].to<JsonObject>(), OCFRFID::insideFrequency(), OCFRFID::insideStats());
    addReader(rfid["outside"].to<JsonObject>(), OCFRFID::outsideFrequency(), OCFRFID::outsideStats());
    serializeJsonPretty(doc, outStr);
    doc.clear();
}

void OCFState::getCatsJSON(String& outStr){
    Guard guard;
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& m: cats){
        OCFCat cat = m.second;
        JsonObject obj = arr.createNestedObject();
        obj["name"] = cat.name;
        obj["rfid"] = cat.rfid;
        obj["allowed_in"] = cat.allow_in;
        obj["allowed_out"] = cat.allow_out;
        obj["last_seen"] = cat.last_seen;
        obj["location"] = cat.location;
    }
    serializeJsonPretty(doc, outStr);
    doc.clear();
}
