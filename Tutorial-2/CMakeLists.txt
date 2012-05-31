
set( TUT2_SOURCE
    tutorial-2.cpp
)

set( TUT2_PACKAGES
    ${BOOST_ASIO_PACKAGES}
)

add_executable( tutorial-2 ${TUT2_SOURCE} )
target_link_libraries( tutorial-2 ${TUT2_PACKAGES} )

