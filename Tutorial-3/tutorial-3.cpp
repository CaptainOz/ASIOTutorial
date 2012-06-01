///
/// @file
/// This is a simple, saychronous wget implementation. It accepts a URL as its one parameter,
/// connects to it, downloads the page via asynchronous HTTP, and prints it to stdout.
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
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;
using boost::asio::ip::tcp;

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

// The `io_service` object needs to be maintained throughout the connection, thus we will be using a
// global variable for it. For more normal applications it is better to keep this in whatever class
// will be managing your application's IO.
boost::asio::io_service io_service;

// We will be passing the data around between the asynchronous functions, so we will be using a
// shared pointer to manage destruction for us when the data becomes unused.
typedef boost::shared_ptr< tcp::socket >                socket_ptr;
typedef boost::shared_ptr< string>                      string_ptr;
typedef boost::shared_ptr< boost::array< char, 1024 > > array_ptr;

void resolveHandler(
    const boost::system::error_code&    error,
    tcp::resolver::iterator             endpoints,
    string                              url
);
void connectHandler(
    const boost::system::error_code&    error,
    tcp::resolver::iterator             endpoint,
    socket_ptr                          socket,
    string                              url
);
void writeHandler(
    const boost::system::error_code&    error,
    size_t                              bytes_transferred,
    socket_ptr                          socket,
    string_ptr                          writeBuffer
);
void readHandler(
    const boost::system::error_code&    error,
    size_t                              bytes_transferred,
    socket_ptr                          socket,
    array_ptr                           readBuffer
);

void requestPage( const string& url ){
    // Split the URL into parts.
    string service, hostname, path;
    parseURL( url, service, hostname, path );

    // Just like tutorial 1, we start by resolving the hostname and service provided from the
    // command line. Only now we will use the asynchronous version which takes a callback function
    // as its second parameter.
    //
    // NOTE I am using the `boost::asio::placeholder`s, however you could use the normal bind
    //      placeholders. If you are using C++11's `std::bind` then you must use `std::placeholder`s
    //      instead as they are not compatible.
    tcp::resolver           resolver( io_service );
    tcp::resolver::query    query( hostname, service );
    resolver.async_resolve(
        query,
        boost::bind(
            &resolveHandler,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator,
            url
        )
    );

    // Next we run the IO service. Note that we are doing this _after_ triggering an asynchronous
    // functon. This is very important, if the `io_service` has nothing to run then all threads
    // calling `io_service.run()` will exit. If you would like to control when the `io_service`
    // stops simply create a `boost::asio::io_service::work` object before calling
    // `io_service.run()` and then destroy that object when you are done. Note that you only need
    // one `io_service::work` object per `io_service` object.
    io_service.run();
}

void resolveHandler(
    const boost::system::error_code&    error,
    tcp::resolver::iterator             endpoints,
    string                              url
){
    if( error ){
        // This error can occur if there is a network issue or if the provided hostname or service
        // can not be resolved.
        cerr << "Resolver error: " << error << endl;
        exit( RESOLVER_FAILURE );
    }

    // Now that we have resolved the URL we can connect to it. Do note that we must pass along the
    // socket to the handler ourselves, Boost ASIO only provides the error and iterator to the
    // callback.
    socket_ptr socket( new tcp::socket( io_service ) );
    boost::asio::async_connect(
        *socket,
        endpoints,
        boost::bind(
            &connectHandler,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator,
            socket,
            url
        )
    );
}

void connectHandler(
    const boost::system::error_code&    error,
    tcp::resolver::iterator             endpoint,
    socket_ptr                          socket,
    string                              url
){
    if( error ){
        // This error can occur if the other side doesn't accept the connection or if there is a
        // network issue.
        cerr << "Connection error: " << error << endl;
        exit( CONNECTION_FAILURE );
    }

    // Split the URL into parts.
    string service, hostname, path;
    parseURL( url, service, hostname, path );

    // We are connected to the server, so we can send our HTTP request now. With asynchronous
    // reading and writing we must ensure the buffer being read from or written to exists for the
    // duration of the read or write so once more we will be using a shared pointer and passing it
    // along with the socket's shared pointer to the handler.
    string_ptr httpRequest( new string( generateRequest( hostname, path ) ) );
    boost::asio::async_write(
        *socket,
        boost::asio::buffer( *httpRequest ),
        boost::bind(
            &writeHandler,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred,
            socket,
            httpRequest
        )
    );
}

void writeHandler(
    const boost::system::error_code&    error,
    size_t                              bytes_transferred,
    socket_ptr                          socket,
    string_ptr                          writeBuffer
){
    if( error ){
        // This error can occur if there is a network issue.
        cerr << "Write error: " << error << endl;
        exit( WRITE_FAILURE );
    }

    // Now that we've sent our request, lets read our response. Because we're doing asynchronous
    // reading we can not loop until `boost::asio::error::eof` is raised. Instead we'll set up the
    // read handler to recurse into itself until it receives `eof`.
    array_ptr readBuffer( new array_ptr::element_type() );
    boost::asio::async_read(
        *socket,
        boost::asio::buffer( *readBuffer ),
        boost::bind(
            &readHandler,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred,
            socket,
            readBuffer
        )
    );
}

void readHandler(
    const boost::system::error_code&    error,
    size_t                              bytes_transferred,
    socket_ptr                          socket,
    array_ptr                           readBuffer
){
    if( error && error != boost::asio::error::eof ){
        cerr << "Read error: " << error << endl;
        exit( READ_FAILURE );
    }

    // We have received some data, so now we'll write it out.
    cout.write( readBuffer->data(), bytes_transferred );

    // If we haven't reached the end of file yet, trigger another asynchronous read just like the
    // one in the `writeHandler`.
    if( error != boost::asio::error::eof ){
        boost::asio::async_read(
            *socket,
            boost::asio::buffer( *readBuffer ),
            boost::bind(
                &readHandler,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                socket,
                readBuffer
            )
        );
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
    const string& url = checkArgs( argc, argv );
    requestPage( url );
    return SUCCESS;
}

