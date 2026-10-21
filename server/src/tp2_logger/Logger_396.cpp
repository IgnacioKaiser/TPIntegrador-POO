#include "Logger_396.h"
#include "Exceptions_396.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>


Logger_396::Logger_396(
    LogLevel minimum_level,
    const std::string& file_name
)
    : minimum_level(minimum_level),
      file(file_name)
{
}


std::string Logger_396::current_timestamp() const
{
    std::time_t now = std::time(nullptr);

    std::tm time_info;

    localtime_r(
        &now,
        &time_info
    );


    std::stringstream ss;

    ss << std::put_time(
        &time_info,
        "%Y-%m-%d %H:%M:%S"
    );


    return ss.str();
}


void Logger_396::log(
    const LogRecord_396& record
)
{
    if (record.get_level() < minimum_level)
    {
        return;
    }


    file.append_line(
        record.to_csv()
    );


    records.push_back(record);
}


void Logger_396::debug(
    const std::string& message,
    const std::string& module,
    int line
)
{
    LogRecord_396 record(
        current_timestamp(),
        LogLevel::DEBUG,
        message,
        module,
        line,
        "",
        ""
    );

    log(record);
}


void Logger_396::info(
    const std::string& message,
    const std::string& module,
    int line
)
{
    LogRecord_396 record(
        current_timestamp(),
        LogLevel::INFO,
        message,
        module,
        line,
        "",
        ""
    );

    log(record);
}


void Logger_396::warning(
    const std::string& message,
    const std::string& module,
    int line
)
{
    LogRecord_396 record(
        current_timestamp(),
        LogLevel::WARNING,
        message,
        module,
        line,
        "",
        ""
    );

    log(record);
}


void Logger_396::error(
    const std::string& message,
    const std::string& module,
    int line
)
{
    LogRecord_396 record(
        current_timestamp(),
        LogLevel::ERROR,
        message,
        module,
        line,
        "",
        ""
    );

    log(record);
}


void Logger_396::critical(
    const std::string& message,
    const std::string& module,
    int line
)
{
    LogRecord_396 record(
        current_timestamp(),
        LogLevel::CRITICAL,
        message,
        module,
        line,
        "",
        ""
    );

    log(record);
}


void Logger_396::log_external(
    LogLevel level,
    const std::string& message,
    const std::string& user_id,
    const std::string& device_id
)
{
    LogRecord_396 record(
        current_timestamp(),
        level,
        message,
        "",
        0,
        user_id,
        device_id
    );

    log(record);
}


bool Logger_396::load()
{
    std::vector<std::string> lines;


    file.read_lines(lines);


    records.clear();


    for (const std::string& line : lines)
    {
        if (line.empty())
        {
            continue;
        }


        LogRecord_396 record;


        if (!record.from_csv(line))
        {
            throw LoggerException(
                "Registro CSV invalido: " + line
            );
        }


        records.push_back(record);
    }


    return true;
}

std::vector<LogRecord_396>
Logger_396::get_by_level(
    LogLevel level
) const
{
    std::vector<LogRecord_396> result;


    for (const LogRecord_396& record : records)
    {
        if (record.get_level() == level)
        {
            result.push_back(record);
        }
    }


    return result;
}


std::vector<LogRecord_396>
Logger_396::get_by_user(
    const std::string& user_id
) const
{
    std::vector<LogRecord_396> result;


    for (const LogRecord_396& record : records)
    {
        if (record.get_user_id() == user_id)
        {
            result.push_back(record);
        }
    }


    return result;
}


std::vector<LogRecord_396>
Logger_396::get_by_date_range(
    const std::string& from,
    const std::string& to
) const
{
    std::vector<LogRecord_396> result;


    for (const LogRecord_396& record : records)
    {
        std::string timestamp =
            record.get_timestamp();


        if (timestamp >= from &&
            timestamp <= to)
        {
            result.push_back(record);
        }
    }


    return result;
}


std::string Logger_396::format_records(
    const std::vector<LogRecord_396>& selected_records,
    const std::string& format
) const
{
    std::stringstream output;


    /*
     * CSV
     */
    if (format == "CSV")
    {
        output
            << "timestamp,level,message,module,"
            << "line,user_id,device_id"
            << std::endl;


        for (const LogRecord_396& record
             : selected_records)
        {
            output
                << record.to_csv()
                << std::endl;
        }


        return output.str();
    }


    /*
     * JSON
     */
    if (format == "JSON")
    {
        output
            << "["
            << std::endl;


        for (size_t i = 0;
             i < selected_records.size();
             i++)
        {
            output
                << "    "
                << selected_records[i].to_json();


            if (i < selected_records.size() - 1)
            {
                output << ",";
            }


            output << std::endl;
        }


        output
            << "]"
            << std::endl;


        return output.str();
    }


    /*
     * XML
     */
    if (format == "XML")
    {
        output
            << "<logs>"
            << std::endl;

        for (const LogRecord_396& record
             : selected_records)
        {
            output
                << record.to_xml()
                << std::endl;
        }

        output
            << "</logs>"
            << std::endl;

        return output.str();
    }


    throw LoggerException(
        "Formato no soportado: " + format
    );
}


std::string Logger_396::get_content(
    const std::string& format
) const
{
    return format_records(
        records,
        format
    );
}