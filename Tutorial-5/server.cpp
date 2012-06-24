///
/// @file
/// This is a simple asynchronous chat server. Clients can connect to it, change their names, send
/// messages, and disconnect.
///

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <iostream>
#include <list>
#include <sstream>
#include <string>

#include "connection.h"

using namespace std;
using boost::asio::ip::tcp;

enum ErrorCodes {
    SUCCESS = 0,
    BAD_ARGUMENTS
};

void getBytes( istream& stream, const size_t count, string& out ){
    for( size_t i = 0; i < count && stream.good(); ++i ){
        out += (char)stream.get();
    }
}

// ************************************************************************** //

class ByteLimit {
private:
    const size_t m_limit;

public:
    template< typename Iterator >
    pair< Iterator, bool > operator()( Iterator begin, Iterator end ) const {
        return make_pair( begin, (end - begin) >= m_limit );
    }

    explicit ByteLimit( const size_t limit, const size_t preRead = 0 )
        : m_limit( limit ){}
    ByteLimit( const ByteLimit& other )
        : m_limit( other.m_limit ){}
    ~ByteLimit( void ){}
}; // end class ByteLimit

// Required to convince Boost ASIO that our match condition is valid.
namespace boost {
    namespace asio {
        template<> struct is_match_condition< ByteLimit > : boost::true_type {};
    }
}

// ************************************************************************** //

class ClientConnection : public boost::enable_shared_from_this< ClientConnection > {
public:
    typedef boost::shared_ptr< ClientConnection >   Pointer;
    typedef boost::system::error_code               error_code;

    typedef boost::function< void( const error_code&, const string&, const string& ) > MessageHandler;

    struct placeholders {
        static boost::arg< 1 > error;
        static boost::arg< 2 > command;
        static boost::arg< 3 > data;
    };

private:
    static const size_t COMMAND_LENGTH  = 4;
    static const size_t HEADER_SIZE     = (COMMAND_LENGTH * sizeof( char )) + sizeof( int );

    Connection::Pointer m_connection;
    string m_clientName;

    void _headerHandler( const error_code& error, istream& data, MessageHandler handler ){
        if( error ){
            handler( error, "", "" );
        }

        // The first 4 bytes contains the name of the command. This is followed by an integer which
        // gives the size of the data to follow.
        string command;
        string dataSizeStr;
        getBytes( data, COMMAND_LENGTH, command );
        getBytes( data, sizeof( int ), dataSizeStr );
        unsigned int dataSize = (unsigned int)ntohl( *(int*)dataSizeStr.data() );

        // If we have more data to read, do that next.
        if( dataSize ){
            m_connection->readUntil(
                ByteLimit( dataSize ),
                boost::bind(
                    &ClientConnection::_dataHandler,
                    shared_from_this(),
                    Connection::placeholders::error,
                    Connection::placeholders::data,
                    command,
                    dataSize,
                    handler
                )
            );
        }

        // Otherwise call the handler now.
        else {
            handler( error, command, "" );
        }
    }

    void _dataHandler(
        const error_code& error,
              istream& stream,
        const string& command, 
        const size_t dataSize,
        MessageHandler handler
    ){
        // Nothing more to do but callback.
        string data( dataSize, ' ' );
        stream.get( &data.at( 0 ), dataSize + 1 );
        handler( error, command, data );
    }

public:
    ClientConnection( Connection::Pointer& connection, const string& clientName )
        : m_connection( connection ),
          m_clientName( clientName ){}

    void readMessage( MessageHandler handler ){
        m_connection->readUntil(
            ByteLimit( HEADER_SIZE ),
            boost::bind(
                &ClientConnection::_headerHandler,
                shared_from_this(),
                Connection::placeholders::error,
                Connection::placeholders::data,
                handler
            )
        );
    }

    void writeMessage( Connection::WriteBuffer message ){
        m_connection->write( message, NULL );
    }

    void setName( const string& name ){
        m_clientName = name;
    }

    const string& getName( void ) const {
        return m_clientName;
    }

    void close( void ){
        m_connection->close();
    }

}; // end class ClientConnection

// ************************************************************************** //

class Server {
public:
    static const unsigned short CHAT_PORT = 8888;
    static const string&        DEFAULT_NAME;

private:
    typedef boost::system::error_code           error_code;
    typedef list< ClientConnection::Pointer >   client_list;

    boost::asio::io_service m_ioService;
    tcp::acceptor           m_acceptor;
    client_list             m_clientList;

    void _accept( void ){
        Connection::Pointer connection( new Connection( m_ioService ) );
        m_acceptor.async_accept(
            connection->getSocket(),
            boost::bind(
                &Server::_acceptHandler,
                this,
                boost::asio::placeholders::error,
                connection
            )
        );
    }

    void _acceptHandler( const error_code& error, Connection::Pointer connection ){
        _accept();

        if( error ){
            cerr << "Client error on accept: " << error.message() << endl;
        }
        else {
            ClientConnection::Pointer client( new ClientConnection( connection, DEFAULT_NAME ) );
            m_clientList.push_front( client );
            _readMessage( client );
        }
    }

    void _readMessage( ClientConnection::Pointer client ){
        client->readMessage(
            boost::bind(
                &Server::_commandHandler,
                this,
                client,
                ClientConnection::placeholders::error,
                ClientConnection::placeholders::command,
                ClientConnection::placeholders::data
            )
        );
    }

    void _commandHandler(
        ClientConnection::Pointer client,
        const error_code& error,
        const string& command,
        const string& data
    ){
        if( error ){
            cerr << "Read command error: " << error.message() << endl;
            client->close();
            m_clientList.remove( client );
            return;
        }

        // Handle the command we just received.
        if( command == "name" ){
            _nameHandler( client, data );
        }
        else if( command == "chat" ){
            _chatHandler( client, data );
        }
        else if( command == "quit" ){
            _quitHandler( client );
        }
        else {
            _unknownCommandHandler( client, command );
        }

        // Then set up the next read.
        _readMessage( client );
    }

    void _nameHandler( ClientConnection::Pointer client, const string& data ){
        client->setName( data );
    }

    void _chatHandler( ClientConnection::Pointer client, const string& data ){
        // NOTE We only need one message to share with all of the clients. The shared pointer will
        //      handle deallocating it for us once the last write has finished.
        Connection::WriteBuffer message( new string( client->getName() + ": " + data + "\n" ) );
        for( client_list::iterator it = m_clientList.begin(); it != m_clientList.end(); ++it ){
            // Don't broadcast the message to the one who sent it.
            if( *it != client ){
                (**it).writeMessage( message );
            }
        }
    }

    void _quitHandler( ClientConnection::Pointer client ){
        client->close();
        m_clientList.remove( client );
    }

    void _unknownCommandHandler( ClientConnection::Pointer client, const string& command ){
        cerr << "Unknown command \"" << command << "\" issued by " << client->getName() << endl;
    }

public:
    Server( void ) : m_acceptor( m_ioService, tcp::endpoint( tcp::v4(), CHAT_PORT ) ){
    }

    void start( void ){
        _accept();
        m_ioService.run();
    }
};
const string& Server::DEFAULT_NAME = "<unknown>";



int main( void ){
    Server server;
    server.start();
}

