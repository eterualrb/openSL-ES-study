cmake_minimum_required(VERSION 3.4.1)

find_library( log-lib
              log )

find_library( opensles-lib
              OpenSLES )

set(wavlib_shared_DIR ${CMAKE_SOURCE_DIR}/../../../libs/${ANDROID_ABI})

add_library( wavlib
             SHARED
             IMPORTED )
set_target_properties( wavlib
                       PROPERTIES IMPORTED_LOCATION
                       ${wavlib_shared_DIR}/libwavlib_shared.so )

include_directories( ${CMAKE_SOURCE_DIR}/
                     ${CMAKE_SOURCE_DIR}/include )

add_library( audio-play
             SHARED
             audioplay.cpp )

target_link_libraries( audio-play
                       ${log-lib}
                       ${opensles-lib}
                       wavlib )