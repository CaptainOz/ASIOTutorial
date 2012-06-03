
Tutorial 4: Asynchronous HTTP Server
====================================

This tutorial creates a simple HTTP server that receives connections, parses HTTP GET requests, and
responds with a file. The files will be resolved relative to a directory passed in on the command
line when the server starts.

Again we will be passing the data around between the asynchronous functions, so we will be using a
shared pointer to manage destruction for us when the data becomes unused.

```cpp
    typedef boost::shared_ptr< tcp::socket >                socket_ptr;
    typedef boost::shared_ptr< string >                     string_ptr;
    typedef boost::shared_ptr< boost::asio::streambuf >     streambuf_ptr;
    typedef boost::shared_ptr< boost::array< char, 1024 > > buffer_ptr;
```

For this tutorial I am going to be using a server class to maintain all data we need. This is the
more normal way to organize an application and doesn't clutter the global space.

```cpp
    class Server {
    private:
```

We still need an `io_service` object, obviously. In addition we'll keep a single `acceptor` and the
path to the root of our resource drive.

```cpp
        boost::asio::io_service m_io_service;
        tcp::acceptor m_acceptor;
        const string m_pathToRoot;
```

The asynchronous accept method will call back once a new connection has arrived or if there is an
error.

```cpp
        void _accept( void ){
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
```

Immediately set up another acceptor. Since we are doing things asynchronously this call will not
block and we'll be ready to accept the next connection right away.

```cpp
        void _acceptHandler( const boost::system::error_code& error, socket_ptr socket ){
            _accept();
```

Just like with the synchronous version we are reading the whole message into memory and then sending
it off to be parsed. Here we are doing it asynchronously, and like all the other Boost ASIO
asynchronous methods that means making sure the socket and buffer remain valid during the operation.

```cpp
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
```

Now send our response back to the client. Nothing new here.

```cpp
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
```

At the end, shut down the socket. We aren't supporting connection: keep-alive with this server.

```cpp
        void _writeHandler(
            const boost::system::error_code& error,
            size_t bytes_transferred,
            socket_ptr socket,
            string_ptr response
        ){
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
```

The constructor for our server just sets up an accept and starts running the `io_service`.

```cpp
    public:
        Server( const string& pathToRoot )
            : m_acceptor( m_io_service, tcp::endpoint( tcp::v4(), HTTP_PORT ) ),
              m_pathToRoot( pathToRoot )
        {
            _accept();
            m_io_service.run();
        }
    }; // end class Server
```

