///
/// @file
/// This is a simple, asychronous HTTP server implementation. It accepts a file path as its one and
/// only parameter. This file path will be used to resolve HTTP get requests.
///
/// @note   The meat of this tutorial is in the Server class.
///
/// @note   I use small try-catch blocks throughout the code in order to better illustrate where
///         Boost::ASIO throws exceptions and document their causes. This is, obviously, not a
///         requirement. Also, any function which may throw an exception can optionally take a
///         boost::system::error_code& as the last parameter. In this case the error will be passed
///         back through that parameter instead of an exception being thrown.
///

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using boost::asio::ip::tcp;

enum ErrorCodes {
    SUCCESS = 0,
    BAD_ARGUMENTS,
    ACCEPTOR_FAILURE,
    CONNECTION_FAILURE,
    WRITE_FAILURE,
    READ_FAILURE,
    SOCKET_CLOSE_FAILURE,
    UNSUPPORTED_REQUEST
};

const unsigned short HTTP_PORT = 80;

string generateResponse( const string& pathToRoot, stringstream& request );

// Again we will be passing the data around between the asynchronous functions, so we will be using
// a shared pointer to manage destruction for us when the data becomes unused.
typedef boost::shared_ptr< tcp::socket >                socket_ptr;
typedef boost::shared_ptr< string >                     string_ptr;
typedef boost::shared_ptr< boost::asio::streambuf >     streambuf_ptr;
typedef boost::shared_ptr< boost::array< char, 1024 > > buffer_ptr;

// For this tutorial I am going to be using a server class to maintain all data we need. This is the
// more normal way to organize an application and doesn't clutter the global space.
class Server {
private:
    // We still need an `io_service` object, obviously. In addition we'll keep a single `acceptor`
    // and the path to the root of our resource drive.
    boost::asio::io_service m_io_service;
    tcp::acceptor m_acceptor;
    const string m_pathToRoot;

    void _accept( void ){
        // The asynchronous accept method will call back once a new connection has arrived or if
        // there is an error.
        socket_ptr socket( new tcp::socket( m_io_service ) );
        m_acceptor.async_accept(
            *socket,
            boost::bind(
                &Server::_acceptHandler,
                this,
                boost::asio::placeholders::error,
                socket
            )
        );
    }

    void _acceptHandler( const boost::system::error_code& error, socket_ptr socket ){
        // Immediately set up another acceptor. Since we are doing things asynchronously this call
        // will not block and we'll be ready to accept the next connection right away.
        _accept();

        // Just like with the synchronous version we are reading the whole message into memory and
        // then sending it off to be parsed. Here we are doing it asynchronously, and like all the
        // other Boost ASIO asynchronous methods that means making sure the socket and buffer remain
        // valid during the operation.
        streambuf_ptr readBuffer( new boost::asio::streambuf );
        boost::asio::async_read_until(
            *socket,
            *readBuffer,
            "\r\n\r\n",
            boost::bind(
                &Server::_readHandler,
                this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                socket,
                readBuffer
            )
        );
    }
    
    void _readHandler(
        const boost::system::error_code& error,
        size_t bytes_transferred,
        socket_ptr socket,
        streambuf_ptr readBuffer
    ){
        // Convert the buffer into a stringstream.
        istream stream( readBuffer.get() );
        stringstream request;
        stream >> request.rdbuf();

        // Now send our response back to the client. Nothing new here.
        string_ptr response( new string( generateResponse( m_pathToRoot, request ) ) );
        boost::asio::async_write(
            *socket,
            boost::asio::buffer( *response ),
            boost::bind(
                &Server::_writeHandler,
                this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                socket,
                response
            )
        );
    }

    void _writeHandler(
        const boost::system::error_code& error,
        size_t bytes_transferred,
        socket_ptr socket,
        string_ptr response
    ){
        // Finally, shut down the socket. We aren't supporting connection: keep-alive with this
        // server.
        try {
            socket->shutdown( tcp::socket::shutdown_both );
            socket->close();
        }
        catch( boost::system::system_error& error ){
            // This error occurs if the read or write streams could not be shutdown for some reason.
            cerr << "Socket close error: " << error.what() << endl;
            exit( SOCKET_CLOSE_FAILURE );
        }
    }

public:
    Server( const string& pathToRoot )
        : m_acceptor( m_io_service, tcp::endpoint( tcp::v4(), HTTP_PORT ) ),
          m_pathToRoot( pathToRoot )
    {
        _accept();
        m_io_service.run();
    }
}; // end class Server

// -------------------------------------------------------------------------- //

string parseRequest( stringstream& request ){
    // We only care about the first line of the request for our simple HTTP server. This line tells
    // us the method (GET, POST, DELETE, etc) and the file name. Our server only supports GET, so
    // check for that first.
    string method;
    getline( request, method );
    if( method.find( "GET " ) != 0 ){
        cerr << "Unsupported HTTP method: " << method << endl;
        exit( UNSUPPORTED_REQUEST );
    }

    // We know it is a get, so extract the file name.
    int httpStart = method.find( " HTTP" );
    return method.substr( 4, httpStart - 4 );
}

string generate404Response( void ){
    return
        "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "\r\n";
}

string generateResponse( const string& pathToRoot, stringstream& request ){
    // Get the filename from the request and open it.
    const string& filename = pathToRoot + parseRequest( request );
    ifstream file( filename.c_str() );
    if( !file ){
        return generate404Response();
    }
    size_t filesize = 0;

    {
        struct stat filestatus;
        stat( filename.c_str(), &filestatus );
        filesize = filestatus.st_size;
    }

    // Now generate the header.
    stringstream response;
    response
        << "HTTP/1.1 200 OK\r\n"
        << "X-Powered-By: Boost ASIO\r\n"
        << "Connection: close\r\n"
        << "Content-Length: " << filesize << "\r\n"
        << "\r\n";
    file >> response.rdbuf();
    return response.str();
}

/// Check that the application arguments are correct and return the only argument this application
/// accepts.
///
/// @param argc The number of arguments the application received.
/// @param argv The command line arguments.
///
/// @return The first (and only) application argument.
string checkArgs( const int argc, char* argv[] ){
    if( argc != 2 ){
        cerr << "Usage: " << argv[0] << " <path to root>" << endl;
        exit( BAD_ARGUMENTS );
    }
    return argv[1];
}

int main( int argc, char* argv[] ){
    const string& pathToRoot = checkArgs( argc, argv );
    Server server( pathToRoot );
    return SUCCESS;
}

