
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"

#ifndef MAKEDEPEND
#include <strings.h>
#include <string.h>

#if defined(_WINDOWS)
# include <stdio.h>

# include <winsock2.h>
//# pragma lib(WS2_32.lib)

#else
extern "C" {
# include <unistd.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <errno.h>
# include <fcntl.h>
}
#endif  // _WINDOWS

#endif // MAKEDEPEND


using namespace XmlRpc;



#if defined(_WINDOWS)
  
static void initWinSock()
{
  static bool wsInit = false;
  if (! wsInit)
  {
    WORD wVersionRequested = MAKEWORD( 2, 0 );
    WSADATA wsaData;
    WSAStartup(wVersionRequested, &wsaData);
    wsInit = true;
  }
}

#else

#define initWinSock()

#endif // _WINDOWS


// These errors are not considered fatal for an IO operation; the operation will be re-tried.

static inline bool

nonFatalError()

{

  int err = XmlRpcSocket::getError();

#if defined(_WINDOWS)
  return err == WSAEINPROGRESS || err == WSAEWOULDBLOCK || err == WSAEINTR;
#else
  return err == EINPROGRESS || err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
#endif

}






SocketHandle
XmlRpcSocket::socket()
{
  initWinSock();
  return (SocketHandle) ::socket(AF_INET, SOCK_STREAM, 0);
}


void
XmlRpcSocket::close(SocketHandle fd)
{
  XmlRpcUtil::log(4, "XmlRpcSocket::close: fd %llu.", static_cast<unsigned long long>(fd));
#if defined(_WINDOWS)
  closesocket(fd);
#else
  ::close(fd);
#endif // _WINDOWS
}




bool
XmlRpcSocket::setNonBlocking(SocketHandle fd)
{
#if defined(_WINDOWS)
  unsigned long flag = 1;
  return (ioctlsocket((SOCKET)fd, FIONBIO, &flag) == 0);
#else
  return (fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
#endif // _WINDOWS
}


bool
XmlRpcSocket::setReuseAddr(SocketHandle fd)
{
  // Allow this port to be re-bound immediately so server re-starts are not delayed
  int sflag = 1;
  return (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&sflag, sizeof(sflag)) == 0);
}


// Bind to a specified port
bool 
XmlRpcSocket::bind(SocketHandle fd, int port)
{
  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons((u_short) port);
  return (::bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == 0);
}


// Set socket in listen mode
bool 
XmlRpcSocket::listen(SocketHandle fd, int backlog)
{
  return (::listen(fd, backlog) == 0);
}


SocketHandle
XmlRpcSocket::accept(SocketHandle fd)
{
  struct sockaddr_in addr;
#if defined(_WINDOWS)
  int
#else
  socklen_t
#endif
    addrlen = sizeof(addr);

  return (SocketHandle) ::accept(fd, (struct sockaddr*)&addr, &addrlen);
}


    
// Connect a socket to a server (from a client)
bool
XmlRpcSocket::connect(SocketHandle fd, std::string& host, int port)
{
  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;

  struct hostent *hp = gethostbyname(host.c_str());
  if (hp == 0) return false;

  saddr.sin_family = hp->h_addrtype;
  memcpy(&saddr.sin_addr, hp->h_addr, hp->h_length);
  saddr.sin_port = htons((u_short) port);

  // For asynch operation, this will return EWOULDBLOCK (windows) or
  // EINPROGRESS (linux) and we just need to wait for the socket to be writable...
  int result = ::connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
  return result == 0 || nonFatalError();
}



// Read available text from the specified socket. Returns false on error.
bool 
XmlRpcSocket::nbRead(SocketHandle fd, std::string& s, bool *eof)
{
  const int READ_SIZE = 4096;   // Number of bytes to attempt to read at a time
  char readBuf[READ_SIZE];

  bool wouldBlock = false;
  *eof = false;

  while ( ! wouldBlock && ! *eof) {
#if defined(_WINDOWS)
    int n = recv(fd, readBuf, READ_SIZE-1, 0);
#else
    int n = read(fd, readBuf, READ_SIZE-1);
#endif
    XmlRpcUtil::log(5, "XmlRpcSocket::nbRead: read/recv returned %d.", n);


    if (n > 0) {
      readBuf[n] = 0;
      s.append(readBuf, n);
    } else if (n == 0) {
      *eof = true;
    } else if (nonFatalError()) {
      wouldBlock = true;
    } else {
      return false;   // Error
    }
  }
  return true;
}


// Write text to the specified socket. Returns false on error.
bool 
XmlRpcSocket::nbWrite(SocketHandle fd, std::string& s, int *bytesSoFar)
{
  int nToWrite = int(s.length()) - *bytesSoFar;
  char *sp = const_cast<char*>(s.c_str()) + *bytesSoFar;
  bool wouldBlock = false;

  while ( nToWrite > 0 && ! wouldBlock ) {
#if defined(_WINDOWS)
    int n = send(fd, sp, nToWrite, 0);
#else
    int n = write(fd, sp, nToWrite);
#endif
    XmlRpcUtil::log(5, "XmlRpcSocket::nbWrite: send/write returned %d.", n);

    if (n > 0) {
      sp += n;
      *bytesSoFar += n;
      nToWrite -= n;
    } else if (nonFatalError()) {
      wouldBlock = true;
    } else {
      return false;   // Error
    }
  }
  return true;
}


// Returns last errno
int 
XmlRpcSocket::getError()
{
#if defined(_WINDOWS)
  return WSAGetLastError();
#else
  return errno;
#endif
}


// Returns message corresponding to last errno
std::string 
XmlRpcSocket::getErrorMsg()
{
  return getErrorMsg(getError());
}

// Returns message corresponding to errno... well, it should anyway
std::string 
XmlRpcSocket::getErrorMsg(int error)
{
  char err[60];
  snprintf(err,sizeof(err),"error %d", error);
  return std::string(err);
}


