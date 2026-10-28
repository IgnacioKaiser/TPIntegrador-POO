#include "domain/CanalSerie.h"

#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

bool PuertoSerie::conectar(const std::string& puerto) {
    if (estaConectado()) return true;
#ifdef _WIN32
    std::string nombre = puerto;
    if (nombre.size() >= 4 && nombre.compare(0, 3, "COM") == 0 && nombre[3] >= '0' && nombre[3] <= '9')
        nombre = "\\\\.\\" + nombre;
    HANDLE abierto = CreateFileA(nombre.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (abierto == INVALID_HANDLE_VALUE) return false;
    DCB config{};
    config.DCBlength = sizeof(config);
    if (!GetCommState(abierto, &config)) { CloseHandle(abierto); return false; }
    config.BaudRate = CBR_115200;
    config.ByteSize = 8; config.Parity = NOPARITY; config.StopBits = ONESTOPBIT;
    config.fBinary = TRUE; config.fOutxCtsFlow = FALSE; config.fOutxDsrFlow = FALSE;
    config.fDtrControl = DTR_CONTROL_ENABLE; config.fRtsControl = RTS_CONTROL_DISABLE;
    config.fOutX = FALSE; config.fInX = FALSE;
    COMMTIMEOUTS tiempos{};
    tiempos.ReadIntervalTimeout = 50;
    tiempos.ReadTotalTimeoutConstant = 100;
    tiempos.WriteTotalTimeoutConstant = 1000;
    if (!SetCommState(abierto, &config) || !SetCommTimeouts(abierto, &tiempos) ||
        !SetupComm(abierto, 4096, 4096)) { CloseHandle(abierto); return false; }
    descriptor_ = reinterpret_cast<std::intptr_t>(abierto);
#else
    const int abierto = open(puerto.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (abierto < 0) return false;
    termios config{};
    if (tcgetattr(abierto, &config) != 0) { close(abierto); return false; }
    cfsetospeed(&config, B115200); cfsetispeed(&config, B115200);
    config.c_cflag = (config.c_cflag & ~CSIZE) | CS8;
    config.c_cflag |= CLOCAL | CREAD;
    config.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    config.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    config.c_oflag &= ~(OPOST | ONLCR | OCRNL);
    config.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    config.c_cc[VMIN] = 0; config.c_cc[VTIME] = 0;
    if (tcsetattr(abierto, TCSANOW, &config) != 0) { close(abierto); return false; }
    descriptor_ = abierto;
#endif
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Solo se descarta la salida de arranque, nunca entre orden y barrera M114.
#ifdef _WIN32
    const bool limpio = PurgeComm(reinterpret_cast<HANDLE>(descriptor_), PURGE_RXCLEAR | PURGE_TXCLEAR) != 0;
#else
    const bool limpio = tcflush(static_cast<int>(descriptor_), TCIOFLUSH) == 0;
#endif
    if (!limpio) desconectar();
    return limpio;
}

void PuertoSerie::desconectar() noexcept {
    if (estaConectado()) {
#ifdef _WIN32
        CloseHandle(reinterpret_cast<HANDLE>(descriptor_));
#else
        close(static_cast<int>(descriptor_));
#endif
    }
    descriptor_ = -1;
    pendientes_.clear();
}

std::string PuertoSerie::leerRespuesta(unsigned int timeoutMs) {
    const auto limite = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string respuesta;
    while (std::chrono::steady_clock::now() < limite) {
        auto fin = pendientes_.find('\n');
        while (fin != std::string::npos) {
            std::string linea = pendientes_.substr(0, fin);
            pendientes_.erase(0, fin + 1);
            respuesta += linea + '\n';
            if (!linea.empty() && linea.back() == '\r') linea.pop_back();
            if (linea == "OK") return respuesta;
            fin = pendientes_.find('\n');
        }
        // Un controlador defectuoso tampoco puede hacer crecer la respuesta sin limite.
        if (respuesta.size() + pendientes_.size() > 64 * 1024)
            throw ErrorSerie("La respuesta serie supera el limite permitido.", respuesta);
        char buffer[256];
#ifdef _WIN32
        DWORD cantidad = 0;
        if (!ReadFile(reinterpret_cast<HANDLE>(descriptor_), buffer, sizeof(buffer), &cantidad, nullptr))
            throw ErrorSerie("Error de I/O al leer el puerto serie.", respuesta);
        const auto n = static_cast<int>(cantidad);
#else
        pollfd espera{static_cast<int>(descriptor_), POLLIN, 0};
        const int listo = poll(&espera, 1, 100);
        if (listo < 0 && errno == EINTR) continue;
        if (listo < 0 || (espera.revents & (POLLERR | POLLHUP | POLLNVAL)))
            throw ErrorSerie("Error de I/O al leer el puerto serie.", respuesta);
        if (listo == 0) continue;
        const auto n = read(static_cast<int>(descriptor_), buffer, sizeof(buffer));
        if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        if (n < 0) throw ErrorSerie("Error de I/O al leer el puerto serie.", respuesta);
#endif
        pendientes_.append(buffer, static_cast<std::size_t>(n));
    }
    throw ErrorSerie("Tiempo de espera agotado sin confirmacion OK del firmware.", respuesta + pendientes_);
}

std::string PuertoSerie::intercambiar(const std::string& comando, unsigned int timeoutMs) {
    if (!estaConectado()) throw ErrorSerie("El puerto serie esta desconectado.");
    try {
        const auto limite = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
        std::size_t enviados = 0;
        while (enviados < comando.size()) {
            if (std::chrono::steady_clock::now() >= limite)
                throw ErrorSerie("Tiempo de espera agotado al escribir en el puerto serie.");
#ifdef _WIN32
            DWORD cantidad = 0;
            if (!WriteFile(reinterpret_cast<HANDLE>(descriptor_), comando.data() + enviados,
                           static_cast<DWORD>(comando.size() - enviados), &cantidad, nullptr) || cantidad == 0)
                throw ErrorSerie("Error de I/O al escribir en el puerto serie.");
#else
            pollfd espera{static_cast<int>(descriptor_), POLLOUT, 0};
            const int listo = poll(&espera, 1, 100);
            if (listo < 0 && errno == EINTR) continue;
            if (listo < 0 || (espera.revents & (POLLERR | POLLHUP | POLLNVAL)))
                throw ErrorSerie("Error de I/O al escribir en el puerto serie.");
            if (listo == 0) continue;
            const auto cantidad = write(static_cast<int>(descriptor_), comando.data() + enviados,
                                        comando.size() - enviados);
            if (cantidad < 0 && (errno == EAGAIN || errno == EINTR)) continue;
            if (cantidad <= 0) throw ErrorSerie("Error de I/O al escribir en el puerto serie.");
#endif
            enviados += static_cast<std::size_t>(cantidad);
        }
        return leerRespuesta(timeoutMs);
    } catch (...) {
        desconectar(); // No asociar una confirmacion tardia con la orden siguiente.
        throw;
    }
}
