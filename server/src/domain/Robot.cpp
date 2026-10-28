#include "domain/Robot.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <regex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

Robot::Robot()
    : fd_serie(-1),
      conectado(false),
      modoTrabajo_(ModoTrabajo::MANUAL),
      modoCoordenadas_(ModoCoordenadas::ABSOLUTO) {}

Robot::~Robot() {
    desconectar();
}

bool Robot::conectar(const std::string& puerto) {
    if (conectado) return true;

#ifdef _WIN32
    std::string nombrePuerto = puerto;
    if (nombrePuerto.size() >= 4 &&
        nombrePuerto.compare(0, 3, "COM") == 0 &&
        nombrePuerto[3] >= '0' && nombrePuerto[3] <= '9') {
        nombrePuerto = "\\\\.\\" + nombrePuerto;
    }

    HANDLE puertoAbierto = CreateFileA(
        nombrePuerto.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (puertoAbierto == INVALID_HANDLE_VALUE) {
        return false;
    }

    DCB configuracion{};
    configuracion.DCBlength = sizeof(configuracion);
    if (!GetCommState(puertoAbierto, &configuracion)) {
        CloseHandle(puertoAbierto);
        return false;
    }
    configuracion.BaudRate = CBR_115200;
    configuracion.ByteSize = 8;
    configuracion.Parity = NOPARITY;
    configuracion.StopBits = ONESTOPBIT;
    configuracion.fBinary = TRUE;
    configuracion.fOutxCtsFlow = FALSE;
    configuracion.fOutxDsrFlow = FALSE;
    configuracion.fDtrControl = DTR_CONTROL_ENABLE;
    configuracion.fRtsControl = RTS_CONTROL_DISABLE;
    configuracion.fOutX = FALSE;
    configuracion.fInX = FALSE;
    if (!SetCommState(puertoAbierto, &configuracion)) {
        CloseHandle(puertoAbierto);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(puertoAbierto, &timeouts) ||
        !SetupComm(puertoAbierto, 4096, 4096)) {
        CloseHandle(puertoAbierto);
        return false;
    }
    fd_serie = reinterpret_cast<std::intptr_t>(puertoAbierto);
#else
    const int puertoAbierto =
        open(puerto.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (puertoAbierto < 0)
        return false;

    struct termios tty;
    if (tcgetattr(puertoAbierto, &tty) != 0) {
        close(puertoAbierto);
        return false;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    tty.c_oflag &= ~(OPOST | ONLCR | OCRNL);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(puertoAbierto, TCSANOW, &tty) != 0) {
        close(puertoAbierto);
        return false;
    }
    fd_serie = puertoAbierto;
#endif

    conectado = true;
    std::this_thread::sleep_for(std::chrono::seconds(2));
#ifdef _WIN32
    if (!PurgeComm(reinterpret_cast<HANDLE>(fd_serie),
                   PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        cerrarPuerto();
        return false;
    }
#else
    if (tcflush(static_cast<int>(fd_serie), TCIOFLUSH) != 0) {
        cerrarPuerto();
        return false;
    }
#endif
    return true;
}

void Robot::desconectar() {
    cerrarPuerto();
}

void Robot::cerrarPuerto() {
    if (fd_serie < 0)
        return;
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(fd_serie));
#else
    close(static_cast<int>(fd_serie));
#endif
    fd_serie = -1;
    conectado = false;
}

bool Robot::estaConectado() const {
    return conectado;
}

// Implementación sugerida por la cátedra para limpiar saltos de línea
std::string Robot::limpiarRespuesta(const std::string& raw) const {
    std::string r = raw;
    r.erase(std::remove(r.begin(), r.end(), '\r'), r.end());
    r.erase(std::remove(r.begin(), r.end(), '\n'), r.end());
    return r;
}

std::string Robot::leerRespuesta(unsigned int timeoutMs) {
    std::string respuesta;
    std::string linea;
    char buffer[256];
    const auto limite =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeoutMs);

    while (true) {
#ifdef _WIN32
        DWORD cantidad = 0;
        if (!ReadFile(reinterpret_cast<HANDLE>(fd_serie), buffer,
                      sizeof(buffer), &cantidad, nullptr)) {
            cerrarPuerto();
            throw std::runtime_error("Error de I/O al leer el puerto serie.");
        }
        const int n = static_cast<int>(cantidad);
#else
        const ssize_t n = read(static_cast<int>(fd_serie), buffer,
                               sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            cerrarPuerto();
            throw std::runtime_error("Error de I/O al leer el puerto serie.");
        }
#endif
        if (n == 0) {
            if (std::chrono::steady_clock::now() < limite)
                continue;
            cerrarPuerto();
            if (respuesta.empty())
                throw std::runtime_error(
                    "Tiempo de espera agotado al leer el puerto serie.");
            const std::string respuestaLimpia = limpiarRespuesta(respuesta);
            validarRespuestaControlador(respuestaLimpia);
            throw std::runtime_error(
                "Respuesta serie incompleta: no se recibio OK del firmware. "
                "Respuesta recibida: " + respuestaLimpia);
        }

        for (int i = 0; i < n; ++i) {
            const char caracter = buffer[i];
            respuesta += caracter;
            if (caracter == '\n') {
                if (limpiarRespuesta(linea) == "OK")
                    return respuesta;
                linea.clear();
            } else {
                linea += caracter;
            }
        }
    }
}

std::string Robot::enviarComandoRaw(
    const std::string& comando,
    unsigned int timeoutMs
) {
    if (!conectado) {
        throw RobotNotConnectedException();
    }
    
#ifdef _WIN32
    if (!PurgeComm(reinterpret_cast<HANDLE>(fd_serie),
                   PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        cerrarPuerto();
        throw std::runtime_error(
            "Error al limpiar el puerto serie antes de escribir.");
    }
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = timeoutMs;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(
            reinterpret_cast<HANDLE>(fd_serie), &timeouts)) {
        cerrarPuerto();
        throw std::runtime_error(
            "Error al configurar el tiempo de espera del puerto serie.");
    }
    std::size_t enviados = 0;
    while (enviados < comando.size()) {
        DWORD cantidad = 0;
        const DWORD restante = static_cast<DWORD>(comando.size() - enviados);
        if (!WriteFile(reinterpret_cast<HANDLE>(fd_serie),
                       comando.data() + enviados, restante, &cantidad, nullptr) ||
            cantidad == 0) {
            cerrarPuerto();
            throw std::runtime_error("Error de I/O al escribir en el puerto serie.");
        }
        enviados += cantidad;
    }
#else
    if (tcflush(static_cast<int>(fd_serie), TCIOFLUSH) != 0) {
        cerrarPuerto();
        throw std::runtime_error(
            "Error al limpiar el puerto serie antes de escribir.");
    }
    std::size_t enviados = 0;
    while (enviados < comando.size()) {
        const ssize_t cantidad = write(
            static_cast<int>(fd_serie), comando.data() + enviados,
            comando.size() - enviados);
        if (cantidad < 0) {
            if (errno == EINTR)
                continue;
            cerrarPuerto();
            throw std::runtime_error(
                "Error de I/O al escribir en el puerto serie.");
        }
        if (cantidad == 0) {
            cerrarPuerto();
            throw std::runtime_error(
                "Escritura serie incompleta.");
        }
        enviados += static_cast<std::size_t>(cantidad);
    }
#endif
    return limpiarRespuesta(leerRespuesta(timeoutMs));
}

std::string Robot::obtenerEstado() {
    return enviarComandoRaw("M114\r");
}

std::string Robot::setMotores(bool activar) {
    std::string comando = activar ? "M17\r" : "M18\r";
    return enviarComandoRaw(comando);
}

ModoTrabajo Robot::modoTrabajo() const {
    return modoTrabajo_;
}

ModoCoordenadas Robot::modoCoordenadas() const {
    return modoCoordenadas_;
}

void Robot::seleccionarModoTrabajo(ModoTrabajo modo) {
    modoTrabajo_ = modo;
}

std::string Robot::seleccionarModoCoordenadas(ModoCoordenadas modo) {
    if (conectado) {
        const std::string comando =
            modo == ModoCoordenadas::ABSOLUTO ? "G90\r" : "G91\r";
        const std::string respuesta = enviarComandoRaw(comando);
        validarRespuestaControlador(respuesta);
        modoCoordenadas_ = modo;
        return respuesta;
    }

    modoCoordenadas_ = modo;
    return "Modo seleccionado; se aplicara al conectar el robot.";
}

void Robot::exigirModoManual() const {
    if (modoTrabajo_ != ModoTrabajo::MANUAL)
        throw RobotModeException(
            "La operacion solo esta disponible en modo manual.");
}

void Robot::validarRespuestaControlador(
    const std::string& respuesta
) const {
    if (respuesta.find("ERROR:") != std::string::npos ||
        respuesta.find("COMMAND NOT RECOGNIZED") != std::string::npos ||
        respuesta.find("POINT IS OUTSIDE OF WORKSPACE") != std::string::npos) {
        throw RobotCommandRejectedException(respuesta);
    }
}

PosicionCartesiana Robot::obtenerPosicionActual() {
    const std::string respuesta = enviarComandoRaw("M114\r");
    validarRespuestaControlador(respuesta);

    static const std::regex formato(
        R"(CURRENT POSITION:\s*\[X:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+Y:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+Z:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+E:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*\])"
    );
    std::smatch coincidencia;
    if (!std::regex_search(respuesta, coincidencia, formato))
        throw std::runtime_error(
            "El controlador no informo una posicion cartesiana valida.");

    PosicionCartesiana posicion{
        std::stod(coincidencia[1].str()),
        std::stod(coincidencia[2].str()),
        std::stod(coincidencia[3].str())
    };
    if (!std::isfinite(posicion.x) || !std::isfinite(posicion.y) ||
        !std::isfinite(posicion.z))
        throw std::runtime_error(
            "El controlador informo una posicion cartesiana invalida.");
    return posicion;
}

bool Robot::posicionPermitida(const PosicionCartesiana& posicion) {
    if (!std::isfinite(posicion.x) || !std::isfinite(posicion.y) ||
        !std::isfinite(posicion.z))
        return false;

    constexpr double longitudBrazoInferior = 120.0;
    constexpr double longitudBrazoSuperior = 120.0;
    constexpr double desplazamientoEfector = 50.0;
    constexpr double cosenoAnguloMinimo = 0.791436948;
    constexpr double cosenoAnguloMaximo = -0.774944489;
    constexpr double alturaMinima = -115.0;
    constexpr double alturaMaxima = 150.0;

    if (posicion.z < alturaMinima || posicion.z > alturaMaxima)
        return false;

    const double radioEfector = std::hypot(posicion.x, posicion.y);
    const double radioBrazo = radioEfector - desplazamientoEfector;
    const double radioCuadrado =
        radioBrazo * radioBrazo + posicion.z * posicion.z;
    const double sumaLongitudesCuadradas =
        longitudBrazoInferior * longitudBrazoInferior +
        longitudBrazoSuperior * longitudBrazoSuperior;
    const double productoLongitudes =
        2.0 * longitudBrazoInferior * longitudBrazoSuperior;
    const double radioMinimoCuadrado =
        sumaLongitudesCuadradas -
        productoLongitudes * cosenoAnguloMinimo;
    const double radioMaximoCuadrado =
        sumaLongitudesCuadradas -
        productoLongitudes * cosenoAnguloMaximo;

    return radioCuadrado >= radioMinimoCuadrado &&
           radioCuadrado <= radioMaximoCuadrado;
}

std::string Robot::irAOrigen() {
    exigirModoManual();
    const std::string respuesta = enviarComandoRaw("G28\r", 10000);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

std::string Robot::mover(
    double x,
    double y,
    double z,
    double velocidad
) {
    exigirModoManual();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        throw std::invalid_argument(
            "Las coordenadas deben ser valores numericos finitos.");
    if (!std::isfinite(velocidad) ||
        velocidad < VELOCIDAD_PREDETERMINADA_MM_S ||
        velocidad > VELOCIDAD_MAXIMA_MM_S) {
        throw std::invalid_argument(
            "La velocidad debe estar entre 5 y 10 mm/s.");
    }

    const PosicionCartesiana actual = obtenerPosicionActual();
    const PosicionCartesiana destino =
        modoCoordenadas_ == ModoCoordenadas::ABSOLUTO
            ? PosicionCartesiana{x, y, z}
            : PosicionCartesiana{actual.x + x, actual.y + y, actual.z + z};
    if (!posicionPermitida(actual))
        throw std::runtime_error(
            "La posicion actual informada por el controlador esta fuera "
            "del espacio de trabajo.");
    if (!posicionPermitida(destino))
        throw std::invalid_argument(
            "La posicion solicitada esta fuera del espacio de trabajo.");

    const double distancia = std::hypot(
        std::hypot(destino.x - actual.x, destino.y - actual.y),
        destino.z - actual.z);
    const int pasos = std::max(1, static_cast<int>(std::ceil(distancia)));
    for (int paso = 1; paso <= pasos; ++paso) {
        const double proporcion =
            static_cast<double>(paso) / static_cast<double>(pasos);
        const PosicionCartesiana intermedia{
            actual.x + (destino.x - actual.x) * proporcion,
            actual.y + (destino.y - actual.y) * proporcion,
            actual.z + (destino.z - actual.z) * proporcion
        };
        if (!posicionPermitida(intermedia))
            throw std::invalid_argument(
                "La trayectoria atraviesa una zona fuera del espacio "
                "de trabajo.");
    }

    const std::string modo =
        modoCoordenadas_ == ModoCoordenadas::ABSOLUTO ? "G90\r" : "G91\r";
    validarRespuestaControlador(enviarComandoRaw(modo));

    std::ostringstream comando;
    comando << std::fixed << std::setprecision(6)
            << "G1 X" << x << " Y" << y << " Z" << z
            << " F" << velocidad << '\r';
    const unsigned int timeoutMs = static_cast<unsigned int>(
        std::ceil(distancia / velocidad * 1000.0) + 10000.0);
    const std::string respuesta =
        enviarComandoRaw(comando.str(), timeoutMs);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

std::string Robot::activarEfector() {
    exigirModoManual();
    const std::string respuesta = enviarComandoRaw("M3\r", 5000);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

std::string Robot::desactivarEfector() {
    exigirModoManual();
    const std::string respuesta = enviarComandoRaw("M5\r", 5000);
    validarRespuestaControlador(respuesta);
    return respuesta;
}
