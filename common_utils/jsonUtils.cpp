#include "jsonUtils.h"

#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

namespace common {

std::string jsonToString(const rapidjson::Value &doc, bool isFormat) {
    if (!isFormat) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    } else {
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }
}

rapidjson::Value strToJson(const std::string &val, rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value strVal;
    strVal.SetString(val.c_str(), val.length(), allocator);
    return strVal;
}

}
