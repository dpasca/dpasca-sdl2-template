project( Demo4 )

include_directories( . )
include_directories( src )
include_directories( ../Common )
# include glm, this is all that it's needed
include_directories( ../../externals/glm )

file( GLOB SRCS "src/*.cpp" )
file( GLOB INCS "src/*.h" )
file( GLOB SRCS_COM "../Common/*.cpp" )
file( GLOB INCS_COM "../Common/*.h" )

# adding this to the exe is only for intellisense, not to compile
file( GLOB INCS_GLM "../externals/glm/glm/*.hpp" )

source_group( Sources FILES ${SRCS} ${INCS} )
source_group( Common FILES ${SRCS_COM} ${INCS_COM} )
source_group( glm FILES ${INCS_GLM} )

add_executable( ${PROJECT_NAME}
                ${SRCS} ${INCS}
                ${SRCS_COM} ${INCS_COM}
                ${INCS_GLM} )

link_directories( ${SDL_LIBRARY_DIRS} )

target_link_libraries( ${PROJECT_NAME}
    ${SDL_LIBRARIES} ${SDL_LIBRARIES_EXTRA} ${PLATFORM_LINK_LIBS} )

Copy_SDL_DLLs_to_RuntimeOut()

