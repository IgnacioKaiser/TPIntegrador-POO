#ifndef APPLICATION_EXCEPTION_H
#define APPLICATION_EXCEPTION_H

#include <stdexcept>
#include <string>

class ApplicationException : public std::runtime_error {
public:
    explicit ApplicationException(const std::string& message)
        : std::runtime_error(message) {}
};

#endif
