/*
Copyright (c) 2018 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma once
#ifndef _SERVER_H_
#define _SERVER_H_

#include "Networking/Headers/tcp_session_tpl.h"

//----------------------------------------------------------------------
namespace Divide {

class Server {
  public:
    Server();
    ~Server();

    void init(U16 port,
              const stringImpl& broadcast_endpoint_address,
              bool debugOutput);

    void close();

  private:
    void handle_accept(tcp_session_ptr session,
                       const boost::system::error_code& ec);

  private:
    boost::asio::io_service io_service_;
    boost::scoped_ptr<std::thread> thread_;
    boost::asio::ip::tcp::acceptor* acceptor_;
    channel _channel;
    bool _debugOutput = false;

};

};  // namespace Divide
#endif
