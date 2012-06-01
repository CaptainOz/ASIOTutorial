
set( TUT3_SOURCE
    tutorial-3.cpp
)

set( TUT3_PACKAGES
    ${BOOST_ASIO_PACKAGES}
)

add_executable( tutorial-3 ${TUT3_SOURCE} )
target_link_libraries( tutorial-3 ${TUT3_PACKAGES} )

