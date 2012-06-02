
Tutorial 2: Synchronous HTTP Server
===================================

This tutorial creates a simple HTTP server that receives connections, parses HTTP GET requests, and
responds with a file. The files will be resolved relative to a directory passed in on the command
line when the server starts.

Every ASIO application needs at least one of these.

```cpp
    boost::asio::io_service io_service;
```

The first thing a server needs is an acceptor. These accept new connections and turn them into
sockets for us.

```cpp
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
```

Now we go into an infinite loop accepting new sockets.

```cpp
    while( true ){
        // The accept method will block until a new connection arrives at the port we are bound to.
        tcp::socket socket( io_service );
        acceptor.accept( socket );
```

Read a line from the socket. The function read_until repeatedly calls read_some on the socket until
the given string is contained within the buffer. Because read_until reads a variable amount of data
it requires a `boost::asio::streambuf` to read into instead of the normal
`boost::asio::mutable_buffer` that the other read functions take.

### NOTE ###
A more efficient way to parse the request would be to do it here rather than reading it all into
memory at once and then parsing it, however this method is fine for demonstrative purposes.

```cpp
        boost::asio::streambuf buffer;
        try {
            boost::asio::read_until( socket, buffer, "\r\n\r\n" );
        }
        catch( boost::system::system_error& error ){
            // This error can occur if there is a network issue.
            cerr << "Read error: " << error.what() << endl;
            exit( READ_FAILURE );
        }
```

The `boost::asio::streambuf` class is compatible with the STL stream classes, so we can easilly get
at the data by just wrapping it in a `std::istream` and then using STL functions for reading lines.

```cpp
        istream stream( &buffer );
        stringstream request;
        stream >> request.rdbuf();
```

Now we have our request, turn it into a response and send it back over the socket to the client.
Note that we don't need to worry about flushing the data here because Boost will handle that for us.
We are guaranteed at the end of of `boost::asio::write` that every byte has been sent.

```cpp
        try {
            const string& response = generateResponse( pathToRoot, request );
            boost::asio::write( socket, boost::asio::buffer( response ) );
        }
        catch( boost::system::system_error& error ){
            // This error can occur if there is a network issue.
            cerr << "Write error: " << error.what() << endl;
            exit( WRITE_FAILURE );
        }
```

We are done with the socket now (this server doesn't support keep-alive), but before we can close
the socket we should shut down its read and write streams. This is not strictly necessary, but good
to do.

```cpp
        try {
            socket.shutdown( tcp::socket::shutdown_both );
        }
        catch( boost::system::system_error& error ){
            // This error occurs if the read or write streams could not be shutdown for some reason.
            cerr << "Shutdown error: " << error.what() << endl;
            exit( SHUTDOWN_FAILURE );
        }
```

Now that all the data is sent and the read and write streams are shut down it is time to close the
socket.

```cpp
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
```

