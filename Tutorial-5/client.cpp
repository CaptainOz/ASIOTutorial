///
/// @file
/// This is an asynchronous chat client. The client takes a server name or IP address as the first
/// parameter.
///

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <string>
#include "connection.h"

using namespace std;
using boost::asio::ip::tcp;

enum ErrorCode {
    SUCCESS = 0,
    BAD_ARGUMENTS,
    RESOLVER_FAILURE,
    READ_FAILURE
};

// ************************************************************************** //

class CharLimit {
private:
    const char m_limitChar;

public:
    CharLimit( const char c ) : m_limitChar( c ){}

    template< typename Iterator >
    pair< Iterator, bool > operator()( Iterator begin, Iterator end ) const {
        Iterator mvr = begin;
        for( ; mvr != end; ++mvr ){
            if( *mvr == m_limitChar ){
                return make_pair( ++mvr, true );
            }
        }
        return make_pair( mvr, false );
    }
}; // end class CharLimit

namespace boost {
    namespace asio {
        template<> struct is_match_condition< CharLimit > : boost::true_type {};
    }
}

// ************************************************************************** //

class ServerConnection : public boost::enable_shared_from_this< ServerConnection > {
public:
    typedef boost::shared_ptr< ServerConnection >   Pointer;
    typedef boost::system::error_code               error_code;

    typedef boost::function< void( const error_code&, const string& ) > MessageHandler;

    struct placeholders {
        static boost::arg< 1 > error;
        static boost::arg< 2 > message;
    };

private:
    Connection::Pointer m_connection;

    void _messageHandler( const error_code& error, istream& stream, MessageHandler handler ){
        string data;
        getline( stream, data );
        handler( error, data );
    }

public:
    ServerConnection( Connection::Pointer& connection ) : m_connection( connection ){}

    void readMessage( MessageHandler handler ){
        m_connection->readUntil(
            CharLimit( '\n' ),
            boost::bind(
                &ServerConnection::_messageHandler,
                shared_from_this(),
                Connection::placeholders::error,
                Connection::placeholders::data,
                handler
            )
        );
    }

    void sendMessage( const string& command, const string& data ){
        // Construct the message string.
        string message = command;
        unsigned int dataSize = (unsigned int)htonl( data.size() );
        message.append( (char *)&dataSize, 4 );
        message.append( data );

        // Send it over the socket.
        m_connection->write( message, NULL );
    }

}; // end class ServerConnection

// ************************************************************************** //

class Client {
public:
    static const string& CHAT_PORT;

private:
    boost::asio::io_service m_ioService;
    tcp::resolver m_resolver;
    ServerConnection::Pointer m_server;
    boost::thread m_thread;

    void _connectToServer( const string& host ){
        cout << "Connecting...";
        tcp::resolver::query query( host, CHAT_PORT );
        m_resolver.async_resolve(
            query,
            boost::bind(
                &Client::_resolveHandler,
                this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::iterator
            )
        );
    }

    void _resolveHandler(
        const   boost::system::error_code&  error,
                tcp::resolver::iterator     endpoints
    ){
        if( error ){
            cerr << "Resolver error: " << error.message() << endl;
            exit( RESOLVER_FAILURE );
        }

        Connection::Pointer connection( new Connection( m_ioService ) );
        boost::asio::async_connect(
            connection->getSocket(),
            endpoints,
            boost::bind(
                &Client::_connectionHandler,
                this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::iterator,
                connection
            )
        );
    }

    void _connectionHandler(
        const   boost::system::error_code&  error,
                tcp::resolver::iterator     endpoint,
                Connection::Pointer&        connection
    ){
        m_server = ServerConnection::Pointer( new ServerConnection( connection ) );
        cout << "done." << endl;
        _readMessage();
    }

    void _readMessage( void ){
        m_server->readMessage(
            boost::bind(
                &Client::_messageHandler,
                this,
                ServerConnection::placeholders::error,
                ServerConnection::placeholders::message
            )
        );
    }

    void _messageHandler( const ServerConnection::error_code& error, const string& message ){
        if( error ){
            cerr << "Message read error: " << error.message() << endl;
            exit( READ_FAILURE );
        }

        cout << message << endl;
        _readMessage();
    }

    void _runIOService( void ){
        m_ioService.run();
    }

    void _readLine( void ){
        string line;
        while( getline( cin, line ) ){
            _parseLine( line );
        }
    }
    
    void _parseLine( const string& line ){
        string command;
        string data = "";
        if( line[0] == '\\' ){
            command = line.substr( 1, 4 );
            if( line.length() > 5 ){
                data = line.substr( 6 );
            }
        }
        else {
            command = "chat";
            data = line;
        }
        m_server->sendMessage( command, data );
    }

public:
    explicit Client( const string& host ) : m_resolver( m_ioService ){
        _connectToServer( host );
    }

    void start( void ){
        m_thread = boost::thread( boost::bind( &Client::_runIOService, this ) );
        _readLine();
    }
}; // end Client

const string& Client::CHAT_PORT = "8888";

// ************************************************************************** //

string checkArgs( int argc, char* argv[] ){
    if( argc != 2 ){
        cerr << "Usage: " << argv[ 0 ] << " <server>" << endl;
        exit( BAD_ARGUMENTS );
    }
    return argv[ 1 ];
}

int main( int argc, char* argv[] ){
    const string& remoteHost = checkArgs( argc, argv );
    Client client( remoteHost );
    client.start();

    return SUCCESS;
}
