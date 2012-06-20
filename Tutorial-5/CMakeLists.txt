
set( TUT5_SERVER_SOURCE
    server.cpp
)

set( TUT5_PACKAGES
    ${BOOST_ASIO_PACKAGES}
)

add_executable( tutorial-5-server ${TUT5_SERVER_SOURCE} )
target_link_libraries( tutorial-5-server ${TUT5_PACKAGES} )

