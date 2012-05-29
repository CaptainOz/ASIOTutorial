///
/// @file
/// This is a simple, sychronous wget immplementation. It accepts a URL as its one parameter,
/// connects to it, downloads the page via synchronous HTTP, and prints it to stdout.
///
/// @note   The meat of this tutorial is in the requestPage method.
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
#include <sstream>
#include <string>

using namespace std;

enum ErrorCodes {
    SUCCESS = 0,
    BAD_ARGUMENTS,
    RESOLVER_FAILURE,
    CONNECTION_FAILURE,
    WRITE_FAILURE,
    READ_FAILURE
};

void parseURL( const string& url, string& service, string& hostname, string& path );
string generateRequest( const string& hostname, const string& path );

void requestPage( const string& url, ostream& out ){
    // Split the URL into parts.
    string service, hostname, path;
    parseURL( url, service, hostname, path );

    // Every Boost::ASIO application needs to have at least one io_service object. Most applications
    // will only need one at all. The io_service object is essentially a work queue which other
    // objects can post tasks to in order to be executed by other threads. For this tutorial we will
    // not be making use of its threadpool functionality, but it is still required by the other ASIO
    // classes.
    boost::asio::io_service io_service;

    // Next we need a resolver. This turns host names and IP strings, via a query object, into a
    // list of endpoints which we can then attempt to connect a socket to.
    using boost::asio::ip::tcp;
    tcp::resolver::iterator endpoint_iterator;
    try {
        tcp::resolver           resolver( io_service );
        tcp::resolver::query    query( hostname, service );
        endpoint_iterator = resolver.resolve( query );
    }
    catch( boost::system::system_error& error ){
        // This error can occur if there is a network issue or if the provided hostname or service
        // can not be resolved.
        cerr << "Resolver error: " << error.what() << endl;
        exit( RESOLVER_FAILURE );
    }

    // Now we can create a socket and connect it using the endpoints provided by the resolver. If
    // the connection works then the socket is automatically opened and ready to send or receive
    // data.
    tcp::socket socket( io_service );
    try {
        boost::asio::connect( socket, endpoint_iterator );
    }
    catch( boost::system::system_error& error ){
        // This error can occur if the other side doesn't accept the connection or if there is a
        // network issue.
        cerr << "Connection error: " << error.what() << endl;
        exit( CONNECTION_FAILURE );
    }

    // We are connected to the server, so we can send our HTTP request now. All reading and writing
    // in Boost::ASIO is done through the boost::asio::mutable_buffer and boost::asio::const_buffer
    // classes, respectively. You do not need to know which one to use or worry about constructing
    // these objects because Boost::ASIO provides the handy boost::asio::buffer method which handles
    // all that for you.
    try {
        const string& httpRequest = generateRequest( hostname, path );
        boost::asio::write( socket, boost::asio::buffer( httpRequest ) );
    }
    catch( boost::system::system_error& error ){
        // This error can occur if there is a network issue.
        cerr << "Write error: " << error.what() << endl;
        exit( WRITE_FAILURE );
    }

    // Now that we've sent our request, lets read our response. Note that Boost::ASIO reports EOF
    // by either throwing or passing back a boost::asio::error::eof error code.
    boost::system::error_code error;
    do {
        boost::array< char, 1024 > buffer;
        size_t bytesRead = socket.read_some( boost::asio::buffer( buffer ), error );
        out.write( buffer.data(), bytesRead );
    } while( !error );

    // This error can occur if there is a network issue.
    if( error != boost::asio::error::eof ){
        cerr << "Read error: " << error << endl;
        exit( READ_FAILURE );
    }
}

// -------------------------------------------------------------------------- //

void parseURL( const string& url, string& service, string& hostname, string& path ){
    try {
        // Service (http/https) is up to the ://.
        size_t serviceEnd = url.find( "://" );
        if( serviceEnd == -1 ){
            throw "Can't find service name.";
        }
        service = url.substr( 0, serviceEnd );
        serviceEnd += 3;

        // Host name is up to the first / after the service name marker.
        size_t hostEnd = url.find( "/", serviceEnd );
        if( hostEnd == -1 ){
            throw "Can't find end of host name.";
        }
        hostname = url.substr( serviceEnd, hostEnd - serviceEnd );

        // Path is everything else.
        path = url.substr( hostEnd );
    }
    catch( const char* error ){
        cerr << "Error parsing url \"" << url << "\": " << error << endl;
        exit( BAD_ARGUMENTS );
    }
}

string generateRequest( const string& hostname, const string& path ){
    stringstream stream;

    stream
        << "GET " << path << " HTTP/1.1\r\n"
        << "Host: " << hostname << "\r\n"
        << "Connection: close" << "\r\n"
        << "\r\n";

    return stream.str();
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
        cerr << "Usage: " << argv[0] << " <url>" << endl;
        exit( BAD_ARGUMENTS );
    }
    return argv[1];
}

int main( int argc, char* argv[] ){
    string url = checkArgs( argc, argv );
    requestPage( url, cout );
    return SUCCESS;
}

