#include "log.h" 

#include <experimental/filesystem>

#ifndef INC_EASY_LOGGING
#error easylogging++.h not include inc.h
#endif

INITIALIZE_EASYLOGGINGPP

namespace fs = std::experimental::filesystem;

namespace common {

void configureLog(const std::string &folder, bool isAppend, bool isConsole, bool isAutoSpacing, bool isTime) {
    const std::string fileName = folder + "/logger.txt";
    const std::string fileNameDebug = folder + "/logger_debug.txt";
    
    if (isAppend) {
        const size_t countBackupFiles = 5;
        
        const auto makeNameFile = [](const std::string &fileName, size_t i) {
            return fileName + "_" + std::to_string(i) + ".txt";
        };
        
        const auto gen = [n = 1, &makeNameFile] (const std::string &fileName) mutable {
            auto res = n++;
            return makeNameFile(fileName, res); 
        };
        
        const auto processOneFile = [&gen, countBackupFiles](const std::string &fileName) {
            std::vector<std::string> files;
            files.push_back(fileName);
            std::generate_n(std::back_inserter(files), countBackupFiles, std::bind(gen, fileName));
            
            std::adjacent_find(files.rbegin(), files.rend(), [](const std::string &curr, const std::string &prev) {
                if (fs::exists(prev)) {
                    fs::rename(prev, curr);
                }
                return false;
            });
        };
        
        processOneFile(fileName);
        processOneFile(fileNameDebug);
    }
    if (fs::exists(fileName)) {
        fs::remove(fileName);
    }
    if (fs::exists(fileNameDebug)) {
        fs::remove(fileNameDebug);
    }
    
    el::Loggers::addFlag(el::LoggingFlag::NewLineForContainer);
    el::Loggers::addFlag(el::LoggingFlag::DisableApplicationAbortOnFatalLog);
    el::Loggers::addFlag(el::LoggingFlag::ImmediateFlush);
    el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
    el::Loggers::addFlag(el::LoggingFlag::MultiLoggerSupport);
    el::Loggers::addFlag(el::LoggingFlag::CreateLoggerAutomatically);
    if (isAutoSpacing) {
        el::Loggers::addFlag(el::LoggingFlag::AutoSpacing);
    } else {
        el::Loggers::removeFlag(el::LoggingFlag::AutoSpacing);
    }
    el::Loggers::addFlag(el::LoggingFlag::IgnoreSigInt);
    
    /*el::Logger* debugLogger = el::Loggers::getLogger("debug");
    (void)debugLogger;*/
    
    el::Configurations defaultConf;
    defaultConf.setToDefault();
    
    std::string timeFormat;
    if (isTime) {
        timeFormat = "%datetime{%H:%m:%s:%g} ";
    }
    
    defaultConf.set(el::Level::Global, el::ConfigurationType::Format, timeFormat + "%level %thread: %msg");
    if (isConsole) {
        defaultConf.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "true");
    } else {
        defaultConf.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
    }
    defaultConf.set(el::Level::Global, el::ConfigurationType::ToFile, "true");
    defaultConf.set(el::Level::Global, el::ConfigurationType::Filename, fileName);
    defaultConf.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, std::to_string(200 * 1024 * 1024));
    
    defaultConf.set(el::Level::Debug, el::ConfigurationType::Format, timeFormat + "%level %thread: %msg");
    if (isConsole) {
        defaultConf.set(el::Level::Debug, el::ConfigurationType::ToStandardOutput, "true");
    } else {
        defaultConf.set(el::Level::Debug, el::ConfigurationType::ToStandardOutput, "false");
    }
    defaultConf.set(el::Level::Debug, el::ConfigurationType::ToFile, "true");
    defaultConf.set(el::Level::Debug, el::ConfigurationType::Filename, fileNameDebug);
    defaultConf.set(el::Level::Debug, el::ConfigurationType::MaxLogFileSize, std::to_string(200 * 1024 * 1024));
    
    el::Loggers::reconfigureLogger("default", defaultConf);
}

void flushLogsAll() {
    el::Loggers::flushAll();
}

}
