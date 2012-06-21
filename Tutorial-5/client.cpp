///
/// @file
///
///

#include <boost/asio.hpp>
#include <iostream>
#include <string>

using namespace std;

enum ErrorCode {
    SUCCESS = 0,
    BAD_ARGUMENTS
};

string checkArgs( int argc, char* argv[] ){
    if( argc != 2 ){
        cerr << "Usage: " << argv[ 0 ] << " <server>" << endl;
        exit( BAD_ARGUMENTS );
    }
    return argv[ 1 ];
}

int main( int argc, char* argv[] ){
    const string& remoteHost = checkArgs( argc, argv );
    return SUCCESS;
}
