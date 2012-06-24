
set( TUT5_SERVER_SOURCE
    connection.h
    server.cpp
)

set( TUT5_CLIENT_SOURCE
    connection.h
    client.cpp
)

set( TUT5_PACKAGES
    ${BOOST_ASIO_PACKAGES}
)

add_executable( tutorial-5-server ${TUT5_SERVER_SOURCE} )
target_link_libraries( tutorial-5-server ${TUT5_PACKAGES} )

add_executable( tutorial-5-client ${TUT5_CLIENT_SOURCE} )
target_link_libraries( tutorial-5-client ${TUT5_PACKAGES} )

