#include "LogRecord_396.h"

#include <sstream>
#include <vector>


LogRecord_396::LogRecord_396()
{
    timestamp = "";
    level = LogLevel::INFO;
    message = "";

    module = "";
    line = 0;

    user_id = "";
    device_id = "";
}


LogRecord_396::LogRecord_396(
    const std::string& timestamp,
    LogLevel level,
    const std::string& message,
    const std::string& module,
    int line,
    const std::string& user_id,
    const std::string& device_id
)
{
    this->timestamp = timestamp;
    this->level = level;
    this->message = message;

    this->module = module;
    this->line = line;

    this->user_id = user_id;
    this->device_id = device_id;
}


std::string LogRecord_396::level_to_string() const
{
    switch (level)
    {
        case LogLevel::DEBUG:
            return "DEBUG";

        case LogLevel::INFO:
            return "INFO";

        case LogLevel::WARNING:
            return "WARNING";

        case LogLevel::ERROR:
            return "ERROR";

        case LogLevel::CRITICAL:
            return "CRITICAL";
    }

    return "UNKNOWN";
}


bool LogRecord_396::string_to_level(
    const std::string& text,
    LogLevel& result
) const
{
    if (text == "DEBUG")
    {
        result = LogLevel::DEBUG;
        return true;
    }

    if (text == "INFO")
    {
        result = LogLevel::INFO;
        return true;
    }

    if (text == "WARNING")
    {
        result = LogLevel::WARNING;
        return true;
    }

    if (text == "ERROR")
    {
        result = LogLevel::ERROR;
        return true;
    }

    if (text == "CRITICAL")
    {
        result = LogLevel::CRITICAL;
        return true;
    }

    return false;
}


bool LogRecord_396::from_csv(
    const std::string& line_csv
)
{
    std::vector<std::string> data;

    std::stringstream ss(line_csv);

    std::string item;


    while (std::getline(ss, item, ','))
    {
        data.push_back(item);
    }


    /*
     * getline no conserva el ultimo campo vacio
     * cuando la linea termina en coma.
     */
    if (!line_csv.empty() &&
        line_csv.back() == ',')
    {
        data.push_back("");
    }


    if (data.size() != 7)
    {
        return false;
    }


    LogLevel parsed_level;

    if (!string_to_level(
            data[1],
            parsed_level
        ))
    {
        return false;
    }


    try
    {
        timestamp = data[0];

        level = parsed_level;

        message = data[2];

        module = data[3];

        line = std::stoi(data[4]);

        user_id = data[5];

        device_id = data[6];
    }
    catch (...)
    {
        return false;
    }


    return true;
}


std::string LogRecord_396::to_csv() const
{
    std::stringstream ss;

    ss << timestamp << ","
       << level_to_string() << ","
       << message << ","
       << module << ","
       << line << ","
       << user_id << ","
       << device_id;

    return ss.str();
}


std::string LogRecord_396::to_json() const
{
    std::stringstream ss;

    ss << "{"
       << "\"timestamp\":\"" << timestamp << "\","
       << "\"level\":\"" << level_to_string() << "\","
       << "\"message\":\"" << message << "\","
       << "\"module\":\"" << module << "\","
       << "\"line\":" << line << ","
       << "\"user_id\":\"" << user_id << "\","
       << "\"device_id\":\"" << device_id << "\""
       << "}";

    return ss.str();
}


std::string LogRecord_396::to_xml() const
{
    std::stringstream ss;

    ss << "<log>" << std::endl;

    ss << "    <timestamp>"
       << timestamp
       << "</timestamp>"
       << std::endl;

    ss << "    <level>"
       << level_to_string()
       << "</level>"
       << std::endl;

    ss << "    <message>"
       << message
       << "</message>"
       << std::endl;

    ss << "    <module>"
       << module
       << "</module>"
       << std::endl;

    ss << "    <line>"
       << line
       << "</line>"
       << std::endl;

    ss << "    <user_id>"
       << user_id
       << "</user_id>"
       << std::endl;

    ss << "    <device_id>"
       << device_id
       << "</device_id>"
       << std::endl;

    ss << "</log>";

    return ss.str();
}


std::string LogRecord_396::get_timestamp() const
{
    return timestamp;
}


LogLevel LogRecord_396::get_level() const
{
    return level;
}


std::string LogRecord_396::get_user_id() const
{
    return user_id;
}