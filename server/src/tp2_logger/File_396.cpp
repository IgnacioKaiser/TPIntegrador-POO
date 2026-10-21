#include "File_396.h"
#include "Exceptions_396.h"

#include <fstream>


File_396::File_396()
{
    initAttributes();
}


File_396::File_396(
    const std::string& file_name
)
{
    initAttributes();

    name = file_name;
}


File_396::~File_396()
{
}


void File_396::initAttributes()
{
    name = "";
}


bool File_396::exist()
{
    std::string path =
        "./data/" + name + ".csv";

    std::ifstream file(path);

    return file.good();
}


bool File_396::append_line(
    const std::string& line
)
{
    std::string path =
        "./data/" + name + ".csv";


    std::ofstream file(
        path,
        std::ios::app
    );


    if (!file.is_open())
    {
        throw FileException(
            "No se pudo abrir el archivo: " + path
        );
    }


    file
        << line
        << std::endl;


    if (file.fail())
    {
        file.close();

        throw FileException(
            "Error al escribir en el archivo: " + path
        );
    }


    file.close();

    return true;
}


bool File_396::read_lines(
    std::vector<std::string>& lines
)
{
    std::string path =
        "./data/" + name + ".csv";


    std::ifstream file(path);


    if (!file.is_open())
    {
        throw FileException(
            "No se pudo abrir el archivo: " + path
        );
    }


    lines.clear();

    std::string line;


    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        lines.push_back(line);
    }


    if (file.bad())
    {
        file.close();

        throw FileException(
            "Error durante la lectura del archivo: " + path
        );
    }


    file.close();

    return true;
}