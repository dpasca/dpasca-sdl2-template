project( Demo1 )

include_directories( . )
include_directories( src )

file( GLOB SRCS "src/*.cpp" )
file( GLOB INCS "src/*.h" )

source_group( Sources FILES ${SRCS} ${INCS} )

add_executable( ${PROJECT_NAME} ${SRCS} ${INCS} )

link_directories( ${SDL_LIBRARY_DIRS} )

target_link_libraries( ${PROJECT_NAME}
    ${SDL_LIBRARIES} ${SDL_LIBRARIES_EXTRA} ${PLATFORM_LINK_LIBS} )

Copy_SDL_DLLs_to_RuntimeOut()

