
Tutorial 3: Asynchrnous wget
============================

This tutorial creates a simple application that connects to a URL provided on the command line,
downloads the page via HTTP asynchronously, and prints it to `stdout`. The application will still be
single threaded however. This tutorial is similar to Tutorial 1: Simple wget, the main difference is
we will be using the `async_*` version of the Boost ASIO functions.

The `io_service` object needs to be maintained throughout the connection, thus we will be using a
global variable for it. For more normal applications it is better to keep this in whatever class
will be managing your application's IO.

```cpp
    boost::asio::io_service io_service;
```

We will be passing the data around between the asynchronous functions, so we will be using a shared
pointer to manage destruction for us when the data becomes unused.

```cpp
    typedef boost::shared_ptr< tcp::socket >                socket_ptr;
    typedef boost::shared_ptr< string>                      string_ptr;
    typedef boost::shared_ptr< boost::array< char, 1024 > > array_ptr;
```

Just like tutorial 1, we start by resolving the hostname and service provided from the command line.
Only now we will use the asynchronous version which takes a callback function as its second
parameter.

#### NOTE ####
I am using the `boost::asio::placeholder`s, however you could use the normal bind placeholders. If
you are using C++11's `std::bind` then you must use `std::placeholder`s instead as they are not
compatible.

```cpp
    void requestPage( const string& url ){
        // Split the URL into parts.
        string service, hostname, path;
        parseURL( url, service, hostname, path );

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
```

Next we run the IO service. Note that we are doing this _after_ triggering an asynchronous functon.
This is very important, if the `io_service` has nothing to run then all threads calling
`io_service.run()` will exit. If you would like to control when the `io_service` stops simply create
a `boost::asio::io_service::work` object before calling `io_service.run()` and then destroy that
object when you are done. Note that you only need one `io_service::work` object per `io_service`
object.

```cpp
        io_service.run();
    }
```

Now that we have resolved the URL we can connect to it. Do note that we must pass along the socket
to the handler ourselves, Boost ASIO only provides the error and iterator to the callback.

```cpp
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
```

We are connected to the server, so we can send our HTTP request now. With asynchronous reading and
writing we must ensure the buffer being read from or written to exists for the duration of the read
or write so once more we will be using a shared pointer and passing it along with the socket's
shared pointer to the handler.

```cpp
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
```

Now that we've sent our request, lets read our response. Because we're doing asynchronous reading we
can not loop until `boost::asio::error::eof` is raised. Instead we'll set up the read handler to
recurse into itself until it receives `eof`.

```cpp
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
```

If we haven't reached the end of file yet, trigger another asynchronous read just like the one in
the `writeHandler`.

```cpp
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
```

```


