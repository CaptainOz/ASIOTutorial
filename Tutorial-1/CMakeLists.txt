
set( TUT1_SOURCE
    tutorial-1.cpp
)

set( TUT1_PACKAGES
    ${BOOST_ASIO_PACKAGES}
)

add_executable( tutorial-1 ${TUT1_SOURCE} )
target_link_libraries( tutorial-1 ${TUT1_PACKAGES} )

