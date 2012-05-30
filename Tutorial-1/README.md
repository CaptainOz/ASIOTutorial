
Tutorial 1: Simple wget
=======================

This tutorial creates a simple application that connects to a URL provided on the command line,
downloads the page via HTTP synchronously, and prints it to `stdout`.

Every Boost::ASIO application needs to have at least one `io_service` object. Most applications will
only need one at all. The `io_service` object is essentially a work queue which other objects can
post tasks to in order to be executed by other threads. For this tutorial we will not be making use
of its threadpool functionality, but it is still required by the other ASIO classes.

```cpp
    boost::asio::io_service io_service;
```

Next we need a resolver. This turns host names and IP strings, via a query object, into a list of
endpoints which we can then attempt to connect a socket to.

```cpp
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
```

Now we can create a socket and connect it using the endpoints provided by the resolver. If the
connection works then the socket is automatically opened and ready to send or receive data.

```cpp
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
```

We are connected to the server, so we can send our HTTP request now. All reading and writing in
Boost::ASIO is done through the `boost::asio::mutable_buffer` and `boost::asio::const_buffer`
classes, respectively. You do not need to know which one to use or worry about constructing these
objects because Boost::ASIO provides the handy `boost::asio::buffer` method which handles all that
for you.

```cpp
    try {
        const string& httpRequest = generateRequest( hostname, path );
        boost::asio::write( socket, boost::asio::buffer( httpRequest ) );
    }
    catch( boost::system::system_error& error ){
        // This error can occur if there is a network issue.
        cerr << "Write error: " << error.what() << endl;
        exit( WRITE_FAILURE );
    }
```

Now that we've sent our request, lets read our response. Note that Boost::ASIO reports EOF by either
throwing or passing back a `boost::asio::error::eof` error code.

```cpp
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
```

And that is it for a simple, synchronous wget implementation using Boost ASIO. The next tutorial
covers a simple synchronous HTTP server supporting just GET.

