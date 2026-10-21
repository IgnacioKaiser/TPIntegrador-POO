#ifndef FILE_396_H
#define FILE_396_H

#include <string>
#include <vector>

class File_396
{
public:
    File_396();
    File_396(const std::string& file_name);
    virtual ~File_396();

    bool exist();

    bool append_line(
        const std::string& line
    );

    bool read_lines(
        std::vector<std::string>& lines
    );

private:
    std::string name;

    void initAttributes();
};

#endif