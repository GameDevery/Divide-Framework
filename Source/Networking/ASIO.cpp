

#ifndef OPCODE_ENUM
#define OPCODE_ENUM OPcodes
#endif

#include "Headers/OPCodesTpl.h"
#include "Headers/ASIO.h"
#include "Headers/Client.h"

#include "Core/Headers/StringHelper.h"
#include "Utility/Headers/Localization.h"

namespace Divide
{

    NO_DESTROY ASIO::LOG_CBK ASIO::s_logCBK;

    ASIO::~ASIO()
    {
        _ioService.stop();
        _work.reset();
        if ( _thread != nullptr )
        {
            _thread->join();
        }
        if ( _localClient != nullptr )
        {
            _localClient->stop();
        }
    }

    void ASIO::disconnect()
    {
        if ( !_connected )
        {
            return;
        }
        WorldPacket p( OPCodes::CMSG_REQUEST_DISCONNECT );
        p << _localClient->getSocket().local_endpoint().address().to_string();
        sendPacket( p );
    }

    bool ASIO::init( const string& address, const U16 port )
    {
        try
        {
            tcp_resolver res( _ioService );
            _localClient = std::make_unique<Client>( this, _ioService, _debugOutput );
            _work.reset( new boost::asio::io_context::work( _ioService ) );
            _localClient->start( res.resolve( address, Util::to_string( port ) ) );
            _thread = std::make_unique<std::thread>( [&]
                                                     {
                                                        SetThreadName("ASIO_THREAD");
                                                         _ioService.run();
                                                     } );
            _ioService.poll();
            _connected = true;
        }
        catch ( const std::exception& e )
        {
            if ( _debugOutput )
            {
                LOG_PRINT(Util::StringFormat( LOCALE_STR("ASIO_EXCEPTION"), e.what()).c_str(), true );
            }
            _connected = false;
        }

        return _connected;
    }

    bool ASIO::connect( const string& address, const U16 port )
    {
        if ( _connected )
        {
            close();
        }

        return init( address, port );
    }

    bool ASIO::isConnected() const noexcept
    {
        return _connected;
    }

    void ASIO::close()
    {
        _localClient->stop();
        _connected = false;
    }

    bool ASIO::sendPacket( WorldPacket& p ) const
    {
        if ( !_connected )
        {
            return false;
        }
        if ( _localClient->sendPacket( p ) )
        {

            if ( _debugOutput )
            {
                LOG_PRINT( Util::StringFormat(LOCALE_STR("ASIO_OPCODE"), p.opcode() ).c_str() );
            }
            return true;
        }

        return false;
    }

    void ASIO::toggleDebugOutput( const bool debugOutput ) noexcept
    {
        _debugOutput = debugOutput;
        _localClient->toggleDebugOutput( _debugOutput );
    }

    void ASIO::SET_LOG_FUNCTION( const LOG_CBK& cbk )
    {
        s_logCBK = cbk;
    }

    void ASIO::LOG_PRINT( const char* msg, const bool error )
    {
        if ( s_logCBK )
        {
            s_logCBK( msg, error );
        }
        else
        {
            if ( error )
            {
                Console::errorfn( msg );
            }
            else
            {
                Console::printfn( msg );
            }
        }
    }

};  // namespace Divide
