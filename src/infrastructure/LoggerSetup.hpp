#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>
#include <memory>
#include <iostream>
#include <filesystem>

class LoggerSetup {
public:
    static void Initialize(const std::string& logDirectory = "logs") {
        try {
            // Ensure the logs directory exists
            std::filesystem::create_directories(logDirectory);
            std::string logFilePath = logDirectory + "/dependency-updater.log";

            // 1. Console Sink (Colored)
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info); 
            // Pattern: [Time] [Level] Message
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

            // 2. Rotating File Sink (Max 5MB per file, keep 3 backup files)
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath, 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::debug); // Keep debug details in the file
            // Pattern: [Time] [Thread] [Level] Message
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");

            // 3. Combine both sinks into the default logger
            std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
            auto multi_logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
            
            // Set global log level to debug (sinks will filter it down if needed)
            multi_logger->set_level(spdlog::level::debug);
            
            // Auto-flush to file instantly if it's a warning or error
            multi_logger->flush_on(spdlog::level::warn); 

            // Register it as the default logger so spdlog::info() uses it automatically
            spdlog::set_default_logger(multi_logger);

            spdlog::info("Logger initialized successfully. Logging to console and {}", logFilePath);

        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }
    
    static void Shutdown() {
        spdlog::shutdown();
    }
};