///
/// @file
/// This is a basic wrapper around a `boost::asio::socket` that provides some helper functions to
/// ease the use of asynchronous reading and writing.
///

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/smart_ptr.hpp>
#include <exception>
#include <string>

using namespace std;
using boost::asio::ip::tcp;

/// A network connection providing a simplified API for reading and writing on a socket.
///
/// All the read/write methods are asynchronous and require a callback that will be called upon
/// completion.
class Connection : public boost::enable_shared_from_this< Connection > {
public:
    /// Buffer for holding a message to be written to the socket.
    typedef boost::shared_ptr< string > WriteBuffer;

    /// Pointer to Connection type.
    typedef boost::shared_ptr< Connection > Pointer;

    /// Connection error type.
    typedef boost::system::error_code Error;

    /// Function prototype for read handlers.
    typedef boost::function< void( const Error&, istream& ) > ReadCallback;

    /// Function prototype for write handlers.
    typedef boost::function< void( const Error&, const size_t ) > WriteCallback;

    struct placeholders {
        static boost::arg< 1 > error;
        static boost::arg< 2 > data;
        static boost::arg< 2 > bytesWritten;
    }; // end struct placeholders

private:
    tcp::socket             m_socket;       ///< Boost::ASIO socket handle.
    boost::asio::streambuf  m_readBuffer;   ///< Read buffer.

    /// Internal read complete handler.
    ///
    /// @param error
    /// @param bytesRead
    /// @param buffer
    /// @param callback
    void _readHandler(
        const Error& error,
        size_t bytesRead,
        ReadCallback& callback
    ){
        if( callback != NULL ){
            istream stream( &m_readBuffer );
            callback( error, stream );
        }
    }

    /// Internal write complete handler.
    ///
    /// @param error
    /// @param bytesWritten
    /// @param buffer
    /// @param callback
    void _writeHandler(
        const Error& error,
        size_t bytesWritten,
        WriteBuffer& buffer,
        WriteCallback& callback
    ){
        if( callback != NULL ){
            callback( error, bytesWritten );
        }
    }

    void _write( WriteBuffer buffer, WriteCallback& callback ){
        boost::asio::async_write(
            m_socket,
            boost::asio::buffer( *buffer ),
            boost::bind(
                &Connection::_writeHandler,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                buffer,
                callback
            )
        );
    }

public:
    /// Constructor.
    ///
    /// @param io_service
    Connection( boost::asio::io_service& io_service )
        : m_socket( io_service ){}

    ~Connection( void ){
        close();
    }

    /// Read from the socket until the given condition is true.
    ///
    /// @tparam Condition
    ///
    /// @param condition
    /// @param callback
    template< typename Condition >
    void readUntil( Condition condition, ReadCallback callback ){
        boost::asio::async_read_until(
            m_socket,
            m_readBuffer,
            condition,
            boost::bind(
                &Connection::_readHandler,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                callback
            )
        );
    }

    /// Write the data buffer to the socket.
    ///
    /// No copy of the data will be made, but a reference will be held internally by the connection
    /// until the write completes.
    ///
    /// @param data
    /// @param callback
    void write( WriteBuffer data, WriteCallback callback ){
        _write( data, callback );
    }

    /// Write the data string to the socket.
    ///
    /// A copy of the data will be made and held internally by the connection until the write
    /// completes.
    ///
    /// @param data
    /// @param callback
    void write( const string& data, WriteCallback callback ){
        _write( WriteBuffer( new string( data ) ), callback );
    }

    tcp::socket& getSocket( void ){
        return m_socket;
    }

    void close( void ){
        if( m_socket.is_open() ){
            try {
                m_socket.shutdown( tcp::socket::shutdown_both );
                m_socket.close();
            }
            catch( const std::exception& e ){
                cerr << "Exception while closing socket: " << e.what() << endl;
            }
        }
    }
};

