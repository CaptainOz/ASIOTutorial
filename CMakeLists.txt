
cmake_minimum_required( VERSION 2.6 )

set( BOOST_INCLUDE_PATH /usr/local/include CACHE PATH "Path to Boost include directory." )
include_directories( ${BOOST_INCLUDE_PATH} )

set( BOOST_LIB_PATH /usr/local/lib CACHE PATH "Path to Boost libraries directory." )
link_directories( ${BOOST_LIB_PATH} )

set( BOOST_ASIO_PACKAGES
    boost_system
    pthread
)

add_subdirectory( Tutorial-1 )
add_subdirectory( Tutorial-2 )
add_subdirectory( Tutorial-3 )
add_subdirectory( Tutorial-4 )
add_subdirectory( Tutorial-5 )

