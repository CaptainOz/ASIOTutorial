
set( TUT4_SOURCE
    tutorial-4.cpp
)

set( TUT4_PACKAGES
    ${BOOST_ASIO_PACKAGES}
)

add_executable( tutorial-4 ${TUT4_SOURCE} )
target_link_libraries( tutorial-4 ${TUT4_PACKAGES} )

