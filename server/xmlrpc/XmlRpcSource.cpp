
#include "XmlRpcSource.h"
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"

namespace XmlRpc {


  XmlRpcSource::XmlRpcSource(SocketHandle fd /*= -1*/, bool deleteOnClose /*= false*/)
    : _fd(fd), _deleteOnClose(deleteOnClose), _keepOpen(false)
  {
  }

  XmlRpcSource::~XmlRpcSource()
  {
  }


  void
  XmlRpcSource::close()
  {
    if (_fd != InvalidSocket) {
      XmlRpcUtil::log(2,"XmlRpcSource::close: closing socket %llu.", static_cast<unsigned long long>(_fd));
      XmlRpcSocket::close(_fd);
      XmlRpcUtil::log(2,"XmlRpcSource::close: done closing socket %llu.", static_cast<unsigned long long>(_fd));
      _fd = InvalidSocket;
    }
    if (_deleteOnClose) {
      XmlRpcUtil::log(2,"XmlRpcSource::close: deleting this");
      _deleteOnClose = false;
      delete this;
    }
  }

} // namespace XmlRpc
