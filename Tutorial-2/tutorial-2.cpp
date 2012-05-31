///
/// @file
/// This is a simple, sychronous HTTP server implementation. It accepts a file path as its one and
/// only parameter. This file path will be used to resolve HTTP get requests.
///
/// @note   The meat of this tutorial is in the runServer method.
///
/// @note   I use small try-catch blocks throughout the code in order to better illustrate where
///         Boost::ASIO throws exceptions and document their causes. This is, obviously, not a
///         requirement. Also, any function which may throw an exception can optionally take a
///         boost::system::error_code& as the last parameter. In this case the error will be passed
///         back through that parameter instead of an exception being thrown.
///

#include <boost/array.hpp>
#include <boost/asio.hpp>
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

enum ErrorCodes {
    SUCCESS = 0,
    BAD_ARGUMENTS,
    ACCEPTOR_FAILURE,
    CONNECTION_FAILURE,
    WRITE_FAILURE,
    READ_FAILURE,
    SHUTDOWN_FAILURE,
    CLOSE_FAILURE,
    UNSUPPORTED_REQUEST
};

const unsigned short HTTP_PORT = 80;

string generateResponse( const string& pathToRoot, stringstream& request );

void runServer( const string& pathToRoot ){
    // Every ASIO application needs at least one of these.
    boost::asio::io_service io_service;

    // The first thing a server needs is an acceptor. These accept new connections and turn them
    // into sockets for us.
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor( io_service );
    try {
        // NOTE This can be done upon construction if you want RAII style code by passing the
        //      endpoint as the second parameter to the constructor. I have done it separately
        //      here simply to isolate the exception that binding can throw.
        acceptor.open( tcp::v4() );
        acceptor.bind( tcp::endpoint( tcp::v4(), HTTP_PORT ) );
        acceptor.listen();
    }
    catch( boost::system::system_error& error ){
        // This error can occur if you do not have permissions to bind to the socket specified (80
        // in our case) or if there is a network issue.
        cerr << "Acceptor error: " << error.what() << endl;
        exit( ACCEPTOR_FAILURE );
    }

    // Now we go into an infinite loop accepting new sockets.
    while( true ){
        // The accept method will block until a new connection arrives at the port we are bound to.
        tcp::socket socket( io_service );
        acceptor.accept( socket );

        // Read a line from the socket. The function read_until repeatedly calls read_some on the
        // socket until the given string is contained within the buffer. Because read_until reads a
        // variable amount of data it requires a `boost::asio::streambuf` to read into instead of
        // the normal `boost::asio::mutable_buffer` that the other read functions take.
        //
        // NOTE A more efficient way to parse the request would be to do it here rather than reading
        //      it all into memory at once and then parsing it, however this method is fine for
        //      demonstrative purposes.
        boost::asio::streambuf buffer;
        try {
            boost::asio::read_until( socket, buffer, "\r\n\r\n" );
        }
        catch( boost::system::system_error& error ){
            // This error can occur if there is a network issue.
            cerr << "Read error: " << error.what() << endl;
            exit( READ_FAILURE );
        }

        // The `boost::asio::streambuf` class is compatible with the STL stream classes, so we can
        // easilly get at the data by just wrapping it in a `std::istream` and then using STL
        // functions for reading lines.
        istream stream( &buffer );
        stringstream request;
        stream >> request.rdbuf();

        // Now we have our request, turn it into a response and send it back over the socket to the
        // client. Note that we don't need to worry about flushing the data here because Boost will
        // handle that for us. We are guaranteed at the end of of `boost::asio::write` that every
        // byte has been sent.
        try {
            const string& response = generateResponse( pathToRoot, request );
            boost::asio::write( socket, boost::asio::buffer( response ) );
        }
        catch( boost::system::system_error& error ){
            // This error can occur if there is a network issue.
            cerr << "Write error: " << error.what() << endl;
            exit( WRITE_FAILURE );
        }

        // We are done with the socket now (this server doesn't support keep-alive), but before we
        // can close the socket we should shut down its read and write streams. This is not strictly
        // necessary, but good to do.
        try {
            socket.shutdown( tcp::socket::shutdown_both );
        }
        catch( boost::system::system_error& error ){
            // This error occurs if the read or write streams could not be shutdown for some reason.
            cerr << "Shutdown error: " << error.what() << endl;
            exit( SHUTDOWN_FAILURE );
        }

        // Now that all the data is sent and the read and write streams are shut down it is time to
        // close the socket.
        try {
            socket.close();
        }
        catch( boost::system::system_error& error ){
            // At this point we are guaranteed the socket is closed, there was just an issue telling
            // the other side to close the socket.
            cerr << "Close error: " << error.what() << endl;

            // NOTE Exiting after a socket close failure is excessive. The server could continue
            //      running at this point with no problem.
            exit( CLOSE_FAILURE );
        }
    }
}

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
    runServer( pathToRoot );
    return SUCCESS;
}

