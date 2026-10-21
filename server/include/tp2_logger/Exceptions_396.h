#ifndef EXCEPTIONS_396_H
#define EXCEPTIONS_396_H

#include <stdexcept>
#include <string>


class FileException : public std::runtime_error
{
public:

    FileException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};


class LoggerException : public std::runtime_error
{
public:

    LoggerException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};


#endif