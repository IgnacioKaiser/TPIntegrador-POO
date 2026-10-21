#include <cstdio>
#include <iostream>
#include <vector>

#include "Logger_396.h"
#include "Exceptions_396.h"


int main()
{
    const std::string file_name =
        "eventos_prueba";


    /*
     * Se elimina solamente el archivo utilizado
     * por esta secuencia de prueba.
     *
     * De esta manera cada ejecucion comienza
     * desde el mismo estado.
     */
    std::remove(
        ("./data/" + file_name + ".csv").c_str()
    );


    try
    {
        // =====================================
        // 1. GENERACION DE EVENTOS
        // =====================================

        {
            Logger_396 logger(
                LogLevel::DEBUG,
                file_name
            );


            // Eventos internos

            logger.debug(
                "Inicio de depuracion",
                "main",
                __LINE__
            );


            logger.info(
                "Aplicacion iniciada",
                "main",
                __LINE__
            );


            logger.warning(
                "Advertencia de prueba",
                "main",
                __LINE__
            );


            logger.error(
                "Error de lectura",
                "File_396",
                __LINE__
            );


            logger.critical(
                "Fallo critico de prueba",
                "Sistema",
                __LINE__
            );


            // Eventos externos

            logger.log_external(
                LogLevel::INFO,
                "Solicitud de estado",
                "usuario_01",
                "dispositivo_01"
            );


            logger.log_external(
                LogLevel::ERROR,
                "Peticion rechazada",
                "usuario_02",
                "dispositivo_02"
            );


            logger.log_external(
                LogLevel::WARNING,
                "Solicitud con advertencia",
                "usuario_01",
                "dispositivo_03"
            );
        }


        // =====================================
        // 2. RECUPERACION DESDE ARCHIVO
        // =====================================

        Logger_396 logger(
            LogLevel::DEBUG,
            file_name
        );


        logger.load();


        // =====================================
        // 3. CONTENIDO COMPLETO
        // =====================================

        std::cout
            << std::endl
            << "=============================="
            << std::endl
            << "CONTENIDO COMPLETO - CSV"
            << std::endl
            << "=============================="
            << std::endl;


        std::cout
            << logger.get_content("CSV")
            << std::endl;


        std::cout
            << "=============================="
            << std::endl
            << "CONTENIDO COMPLETO - JSON"
            << std::endl
            << "=============================="
            << std::endl;


        std::cout
            << logger.get_content("JSON")
            << std::endl;


        std::cout
            << "=============================="
            << std::endl
            << "CONTENIDO COMPLETO - XML"
            << std::endl
            << "=============================="
            << std::endl;


        std::cout
            << logger.get_content("XML")
            << std::endl;


        // =====================================
        // 4. FILTRO POR TIPO DE EVENTO
        // =====================================

        std::vector<LogRecord_396> errors =
            logger.get_by_level(
                LogLevel::ERROR
            );


        std::cout
            << "=============================="
            << std::endl
            << "FILTRO POR NIVEL: ERROR"
            << std::endl
            << "=============================="
            << std::endl;


        std::cout
            << logger.format_records(
                errors,
                "CSV"
            )
            << std::endl;


        // =====================================
        // 5. FILTRO POR USUARIO
        // =====================================

        std::vector<LogRecord_396> user_logs =
            logger.get_by_user(
                "usuario_01"
            );


        std::cout
            << "=============================="
            << std::endl
            << "FILTRO POR USUARIO: usuario_01"
            << std::endl
            << "=============================="
            << std::endl;


        std::cout
            << logger.format_records(
                user_logs,
                "JSON"
            )
            << std::endl;


        // =====================================
        // 6. FILTRO POR RANGO DE FECHAS
        // =====================================

        std::vector<LogRecord_396> date_logs =
            logger.get_by_date_range(
                "2000-01-01 00:00:00",
                "2100-12-31 23:59:59"
            );


        std::cout
            << "=============================="
            << std::endl
            << "FILTRO POR RANGO DE FECHAS"
            << std::endl
            << "=============================="
            << std::endl;


        std::cout
            << logger.format_records(
                date_logs,
                "XML"
            )
            << std::endl;


        // =====================================
        // 7. PRUEBA DEL MANEJO DE EXCEPCIONES
        // =====================================

        std::cout
            << "=============================="
            << std::endl
            << "PRUEBA DE EXCEPCION"
            << std::endl
            << "=============================="
            << std::endl;


        try
        {
            /*
             * PDF no es un formato soportado.
             * Debe producir LoggerException.
             */
            std::cout
                << logger.get_content("PDF")
                << std::endl;
        }
        catch (const LoggerException& e)
        {
            std::cout
                << "Excepcion controlada correctamente: "
                << e.what()
                << std::endl;
        }


        std::cout
            << std::endl
            << "Secuencia de prueba finalizada correctamente."
            << std::endl;
    }


    catch (const FileException& e)
    {
        std::cerr
            << "Error de archivo: "
            << e.what()
            << std::endl;

        return 1;
    }


    catch (const LoggerException& e)
    {
        std::cerr
            << "Error de Logger: "
            << e.what()
            << std::endl;

        return 1;
    }


    catch (const std::exception& e)
    {
        std::cerr
            << "Error general: "
            << e.what()
            << std::endl;

        return 1;
    }


    return 0;
}