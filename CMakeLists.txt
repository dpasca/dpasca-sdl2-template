cmake_minimum_required (VERSION 3.7)

#==================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules")

# specify to use Unity Builds
set(BUILD_UNITY FALSE)

set(USE_SDL TRUE)

#=============================================
project (dpasca-sdl2-template)

macro(Copy_SDL_DLLs_to_RuntimeOut)
    if (USE_SDL AND SDL_SHARED AND MSVC)
        ADD_CUSTOM_COMMAND( TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<$<CONFIG:Release>:${SDL_ROOT_DIR}/$<CONFIG>/SDL2.dll>
            $<$<CONFIG:Debug>:${SDL_ROOT_DIR}/$<CONFIG>/SDL2d.dll>
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>)
    endif()
endmacro()

#=============================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_COMPILER_IS_GNUCXX AND NOT EMSCRIPTEN)
    set(CPPFS_LIBRARIES "stdc++fs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(CMAKE_C_FLAGS   "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# additional compilation flags
if (MSVC)
    set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}       /DDEBUG /D_DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}     /DDEBUG /D_DEBUG")
    set(CMAKE_C_FLAGS_DEBUGOPT    "${CMAKE_C_FLAGS_DEBUGOPT}    /DDEBUG /D_DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUGOPT  "${CMAKE_CXX_FLAGS_DEBUGOPT}  /DDEBUG /D_DEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}       -DDEBUG -D_DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}     -DDEBUG -D_DEBUG")
    set(CMAKE_C_FLAGS_DEBUGOPT    "${CMAKE_C_FLAGS_DEBUGOPT}    -DDEBUG -D_DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUGOPT  "${CMAKE_CXX_FLAGS_DEBUGOPT}  -DDEBUG -D_DEBUG")
endif()

if (MSVC)
    # disable annoying secure CRT warnings
    add_definitions( /D_CRT_SECURE_NO_WARNINGS /wd4996 /nologo )
    #add_compile_options(/W4)
    # suppress class 'type1' needs to have dll-interface to be used by clients of class 'type2'
    add_compile_options(/wd4251)
    add_compile_options(/w34263 /w34266) # enable C4263 and 4266
    add_compile_options(/wd5054)
    add_compile_options(/wd4324) # structure was padded due to alignment specifier
endif()

if (MSVC)
    set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}     /Ox /Ot /Oi /Ob2 /Oy /GS-")
    set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE}   /Ox /Ot /Oi /Ob2 /Oy /GS-")
    set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}       /Od /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}     /Od /Zi")
    set(CMAKE_C_FLAGS_DEBUGOPT    "${CMAKE_C_FLAGS_DEBUGOPT}    /O2 /Zi /Ot /Oi /Ob2")
    set(CMAKE_CXX_FLAGS_DEBUGOPT  "${CMAKE_CXX_FLAGS_DEBUGOPT}  /O2 /Zi /Ot /Oi /Ob2")

    string( REPLACE "/RTC1" ""  CMAKE_C_FLAGS_DEBUGOPT    ${CMAKE_C_FLAGS_DEBUGOPT} )
    string( REPLACE "/RTC1" ""  CMAKE_CXX_FLAGS_DEBUGOPT  ${CMAKE_CXX_FLAGS_DEBUGOPT} )

    add_definitions( "/fp:fast" )            # common params for FPU
    add_definitions( "/DWIN32" )             # some common params
    add_definitions( "/DNOMINMAX" )          # avoid crazy old Windows.h min and max macros
    add_definitions( "/MP" )                 # multi-process compilation
    add_definitions( "/D_WIN32_WINNT=0x0601" )
endif()

##
#if (WIN32)
#    set(GLOBAL_SYS_LIBS ws2_32.lib)
#endif (WIN32)

# Used to copy tools' executables into place
if (WIN32)
	set(EXE_POSTFIX ".exe")
	set(DES_EXE_POSTFIX ".exe")
else ()
	set(EXE_POSTFIX "")
	set(DES_EXE_POSTFIX ".bin")
endif ()

# Common packages
cmake_policy(SET CMP0072 NEW)

# for emscripten it's GLES
if (EMSCRIPTEN)
    set(IS_GLES TRUE)
    set(USE_WEBGL2 TRUE)
endif()

#==================================================================
# externals

if (USE_SDL)
    add_definitions( -DUSE_SDL )

    if (EMSCRIPTEN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2")
    else()
        # NOTE: for some reason the Debug build always goes for dll, so we
        #  must make it. arghh
        set(SDL_SHARED TRUE )
        set(SDL_STATIC FALSE )
        add_subdirectory( externals/SDL2 )

        set(SDL_ROOT_DIR ${CMAKE_BINARY_DIR}/externals/SDL2)

        if (WIN32)
            set(SDL_LIBRARY_DIRS ${SDL_ROOT_DIR}/Release ${SDL_ROOT_DIR}/Debug )

            set(SDL_LIBRARIES
                optimized   ${SDL_ROOT_DIR}/Release/SDL2main.lib
                debug       ${SDL_ROOT_DIR}/Debug/SDL2maind.lib
                optimized   ${SDL_ROOT_DIR}/Release/SDL2.lib
                debug       ${SDL_ROOT_DIR}/Debug/SDL2d.lib )

            # from https://stackoverflow.com/a/39128297/1507238
            # NOTE: updated list comes from cmake output
            set(SDL_LIBRARIES_EXTRA
                user32;gdi32;winmm;imm32;ole32;oleaut32;version;uuid;advapi32;setupapi;shell32;dinput8 )
        elseif (UNIX)
            set(SDL_LIBRARY_DIRS ${SDL_ROOT_DIR})

            set(SDL_LIBRARIES
                optimized   ${SDL_ROOT_DIR}/libSDL2main.a
                debug       ${SDL_ROOT_DIR}/libSDL2maind.a
                optimized   ${SDL_ROOT_DIR}/libSDL2.a
                debug       ${SDL_ROOT_DIR}/libSDL2d.a )

            # for dlclose()
            set(SDL_LIBRARIES_EXTRA ${CMAKE_DL_LIBS})
        endif()

        #message( "SDL_LIBRARY_DIRS = ${SDL_LIBRARY_DIRS}" )
        #message( "SDL_LIBRARIES = ${SDL_LIBRARIES}" )
        #message( "SDL_LIBRARIES_EXTRA = ${SDL_LIBRARIES_EXTRA}" )

        # include path for using code
        include_directories( externals/SDL2/include )
    endif()
endif()

#==================================================================
# Specify the destination for the build products
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../../_bin )

# Various libs
if (NOT EMSCRIPTEN)
    if (APPLE OR UNIX)
        set(PLATFORM_LINK_LIBS -pthread -lpthread)
    endif()
else()
    set(PLATFORM_LINK_LIBS -wWASM=1 -sDISABLE_EXCEPTION_CATCHING=0)
endif()

function(add_subdirectory_if_exists DIR)
    if (EXISTS "${CMAKE_SOURCE_DIR}/${DIR}/CMakeLists.txt")
        add_subdirectory( ${DIR} )
    else()
        message( "Skipping subdir \"${DIR}\"" )
    endif()
endfunction()

# apps
add_subdirectory_if_exists( Apps/Demo1 )

