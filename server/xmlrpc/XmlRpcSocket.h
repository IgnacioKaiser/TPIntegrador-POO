#ifndef _XMLRPCSOCKET_H_
#define _XMLRPCSOCKET_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <string>
# include <cstdint>
#endif

namespace XmlRpc {

  // Windows usa handles del ancho de un puntero; Linux usa int.
#ifdef _WIN32
  using SocketHandle = std::uintptr_t;
#else
  using SocketHandle = int;
#endif
  constexpr SocketHandle InvalidSocket = static_cast<SocketHandle>(-1);

  //! A platform-independent socket API.
  class XmlRpcSocket {
  public:

    //! Creates a stream (TCP) socket. Returns -1 on failure.
    static SocketHandle socket();

    //! Closes a socket.
    static void close(SocketHandle socket);


    //! Sets a stream (TCP) socket to perform non-blocking IO. Returns false on failure.
    static bool setNonBlocking(SocketHandle socket);

    //! Read text from the specified socket. Returns false on error.
    static bool nbRead(SocketHandle socket, std::string& s, bool *eof);

    //! Write text to the specified socket. Returns false on error.
    static bool nbWrite(SocketHandle socket, std::string& s, int *bytesSoFar);


    // The next four methods are appropriate for servers.

    //! Allow the port the specified socket is bound to to be re-bound immediately so 
    //! server re-starts are not delayed. Returns false on failure.
    static bool setReuseAddr(SocketHandle socket);

    //! Bind to a specified port
    static bool bind(SocketHandle socket, int port);

    //! Set socket in listen mode
    static bool listen(SocketHandle socket, int backlog);

    //! Accept a client connection request
    static SocketHandle accept(SocketHandle socket);


    //! Connect a socket to a server (from a client)
    static bool connect(SocketHandle socket, std::string& host, int port);


    //! Returns last errno
    static int getError();

    //! Returns message corresponding to last error
    static std::string getErrorMsg();

    //! Returns message corresponding to error
    static std::string getErrorMsg(int error);
  };

} // namespace XmlRpc

#endif
