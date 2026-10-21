#ifndef LOGGER_396_H
#define LOGGER_396_H

#include <string>
#include <vector>

#include "LogRecord_396.h"
#include "File_396.h"


class Logger_396
{
private:

    LogLevel minimum_level;

    File_396 file;

    std::vector<LogRecord_396> records;


    void log(
        const LogRecord_396& record
    );


    std::string current_timestamp() const;


public:

    Logger_396(
        LogLevel minimum_level,
        const std::string& file_name
    );


    void debug(
        const std::string& message,
        const std::string& module,
        int line
    );


    void info(
        const std::string& message,
        const std::string& module,
        int line
    );


    void warning(
        const std::string& message,
        const std::string& module,
        int line
    );


    void error(
        const std::string& message,
        const std::string& module,
        int line
    );


    void critical(
        const std::string& message,
        const std::string& module,
        int line
    );


    void log_external(
        LogLevel level,
        const std::string& message,
        const std::string& user_id,
        const std::string& device_id
    );


    bool load();


    std::string get_content(
        const std::string& format
    ) const;


    std::vector<LogRecord_396> get_by_level(
        LogLevel level
    ) const;


    std::vector<LogRecord_396> get_by_user(
        const std::string& user_id
    ) const;


    std::vector<LogRecord_396> get_by_date_range(
        const std::string& from,
        const std::string& to
    ) const;


    std::string format_records(
        const std::vector<LogRecord_396>& selected_records,
        const std::string& format
    ) const;
};


#endif