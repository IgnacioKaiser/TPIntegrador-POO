#include "domain/Robot.h"

#include <algorithm>
#include <chrono>
#include <cstring>
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

Robot::Robot() : fd_serie(-1), conectado(false) {}

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

std::string Robot::leerRespuesta() {
    std::string respuesta;
    std::string linea;
    char buffer[256];

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
            cerrarPuerto();
            if (respuesta.empty())
                throw std::runtime_error(
                    "Tiempo de espera agotado al leer el puerto serie.");
            throw std::runtime_error(
                "Respuesta serie incompleta: no se recibio OK del firmware.");
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

std::string Robot::enviarComandoRaw(const std::string& comando) {
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
    return limpiarRespuesta(leerRespuesta());
}

std::string Robot::obtenerEstado() {
    return enviarComandoRaw("M114\r");
}

std::string Robot::setMotores(bool activar) {
    std::string comando = activar ? "M17\r" : "M18\r";
    return enviarComandoRaw(comando);
}
