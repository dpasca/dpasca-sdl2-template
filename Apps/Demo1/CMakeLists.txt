project( Demo1 )

include_directories( . )
include_directories( src )
include_directories( ../Common )

file( GLOB SRCS "src/*.cpp" )
file( GLOB INCS "src/*.h" )
file( GLOB SRCS_COM "../Common/*.cpp" )
file( GLOB INCS_COM "../Common/*.h" )

source_group( Sources FILES ${SRCS} ${INCS} )
source_group( Common FILES ${SRCS_COM} ${INCS_COM} )

add_executable( ${PROJECT_NAME} ${SRCS} ${INCS} ${SRCS_COM} ${INCS_COM} )

target_link_libraries( ${PROJECT_NAME}
    ${SDL_LIBRARIES}
    ${SDL_LIBRARIES_EXTRA}
    ${PLATFORM_LINK_LIBS}
    ${IMGUI_LIBRARIES} )

Copy_SDL_DLLs_to_RuntimeOut()

