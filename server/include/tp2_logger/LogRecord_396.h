#ifndef LOGRECORD_396_H
#define LOGRECORD_396_H

#include <string>


enum class LogLevel
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};


class LogRecord_396
{
private:

    std::string timestamp;
    LogLevel level;
    std::string message;

    std::string module;
    int line;

    std::string user_id;
    std::string device_id;


    std::string level_to_string() const;

    bool string_to_level(
        const std::string& text,
        LogLevel& level
    ) const;


public:

    LogRecord_396();

    LogRecord_396(
        const std::string& timestamp,
        LogLevel level,
        const std::string& message,
        const std::string& module,
        int line,
        const std::string& user_id,
        const std::string& device_id
    );


    bool from_csv(
        const std::string& line
    );


    std::string to_csv() const;

    std::string to_json() const;

    std::string to_xml() const;


    std::string get_timestamp() const;

    LogLevel get_level() const;

    std::string get_user_id() const;
};


#endif