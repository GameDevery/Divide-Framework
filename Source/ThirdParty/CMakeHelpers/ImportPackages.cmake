include(FetchContent)

set(BUILD_TESTING OFF)

#CEGUI
include(ThirdParty/CMakeHelpers/ImportCEGUI.cmake)

include(ThirdParty/CMakeHelpers/FetchContentExcludeFromAll.cmake)

add_compile_definitions(IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
add_compile_definitions(IMGUI_DISABLE_OBSOLETE_KEYIO)
add_compile_definitions(IMGUI_USE_STB_SPRINTF)

#SPIRV-Reflect
FetchContent_Declare(
    spirv_reflect
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect
    GIT_TAG        2f7460f0be0f73c9ffde719bc3e924b4250f4d98
    #GIT_PROGRESS   TRUE
)

set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)

#Optick
FetchContent_Declare(
  optick
  GIT_REPOSITORY https://github.com/bombomby/optick.git
  GIT_TAG        8abd28dee1a4034c973a3d32cd1777118e72df7e
  #GIT_PROGRESS   TRUE
)

set(OPTICK_BUILD_CONSOLE_SAMPLE FALSE)
set(OPTICK_BUILD_GUI_APP FALSE)
set(OPTICK_ENABLED TRUE)
set(OPTICK_INSTALL_TARGETS FALSE)
set(OPTICK_USE_D3D12 FALSE)
set(OPTICK_USE_VULKAN TRUE)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)


#Jolt Physics
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY  https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG         master
    GIT_SHALLOW     TRUE
    #GIT_PROGRESS    TRUE
)

#Skarupke hash maps
FetchContent_Declare(
    skarupke
    GIT_REPOSITORY https://github.com/skarupke/flat_hash_map.git
    GIT_TAG        2c4687431f978f02a3780e24b8b701d22aa32d9c
    #GIT_PROGRESS   TRUE
)

#MemoryPool
FetchContent_Declare(
    memory_pool
    GIT_REPOSITORY https://github.com/cacay/MemoryPool.git
    GIT_TAG        1ab4683e38f24940afb397b06a2d86814d898e63
    #GIT_PROGRESS   TRUE
)

#TileableVolumeNoise
FetchContent_Declare(
    tileable_volume_noise
    GIT_REPOSITORY https://github.com/IonutCava/TileableVolumeNoise.git
    GIT_TAG        master
    #GIT_PROGRESS   TRUE
)

#CurlNoise
FetchContent_Declare(
    curl_noise
    GIT_REPOSITORY https://github.com/IonutCava/CurlNoise.git
    GIT_TAG        master
    #GIT_PROGRESS   TRUE
)

#SimpleFileWatcher
FetchContent_Declare(
    simple_file_watcher
    GIT_REPOSITORY https://github.com/jameswynn/simplefilewatcher.git
    GIT_TAG        4bccd086621698f21a6e95f3ff77c559f862184a
    #GIT_PROGRESS   TRUE
    SYSTEM
)

#fcpp
FetchContent_Declare(
    fcpp
    GIT_REPOSITORY https://github.com/IonutCava/fcpp.git
    GIT_TAG        master
    #GIT_PROGRESS   TRUE
    SYSTEM
)

#imgui_club
FetchContent_Declare(
    imgui_club
    GIT_REPOSITORY https://github.com/ocornut/imgui_club.git
    GIT_TAG        d93fbf853c5648b5c1404b8595b5ab843dd0a74c
    #GIT_PROGRESS   TRUE
)

#imguizmo
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        e552f632bbb17a0ebf5a91a22900f6f68bac6545
    #GIT_PROGRESS   TRUE
)


#IconFontCppHeaders
FetchContent_Declare(
    icon_font_cpp_headers
    GIT_REPOSITORY https://github.com/juliettef/IconFontCppHeaders.git
    GIT_TAG        8886c5657bac22b8fee34354871e3ade2a596433
    #GIT_PROGRESS   TRUE
)

#chaiscript
set(UNIT_TEST_LIGHT TRUE)
set(BUILD_SAMPLES FALSE)
FetchContent_Declare(
    chaiscript
    GIT_REPOSITORY https://github.com/ChaiScript/ChaiScript.git
    GIT_TAG        406a7ba1ef144d67021a68b1ba09224244a761ca
    #GIT_PROGRESS   TRUE
    SYSTEM
)

FetchContent_MakeAvailableExcludeFromAll(
    spirv_reflect
    optick
    skarupke
    memory_pool
    tileable_volume_noise
    curl_noise
    simple_file_watcher
    fcpp
    imgui_club
    imguizmo
    icon_font_cpp_headers
    chaiscript
)

FetchContent_MakeAvailable_JoltPhysics()

if (BUILD_TESTING_INTERNAL)
    set(BUILD_TESTING ON)
endif()

include_directories(
    SYSTEM
    ${spirv-reflect_SOURCE_DIR}
    ${optick_SOURCE_DIR}/src
    ${vk_bootstrap_SOURCE_DIR}/src
    ${skarupke_SOURCE_DIR}
    ${memory_pool_SOURCE_DIR}
    ${tileable_volume_noise_SOURCE_DIR}
    ${curl_noise_SOURCE_DIR}
    ${simple_file_watcher_SOURCE_DIR}/include
    ${fcpp_SOURCE_DIR}
    ${imgui_club_SOURCE_DIR}
    ${imguizmo_SOURCE_DIR}
    ${icon_font_cpp_headers_SOURCE_DIR}
    ${chaiscript_SOURCE_DIR}/include
    ${JoltPhysics_SOURCE_DIR}/..
)

set( TILEABLE_VOLUME_NOISE_SRC_FILES ${tileable_volume_noise_SOURCE_DIR}/TileableVolumeNoise.cpp )

set( CURL_NOISE_SRC_FILES ${curl_noise_SOURCE_DIR}/CurlNoise/Curl.cpp
                          ${curl_noise_SOURCE_DIR}/CurlNoise/Noise.cpp
)

set( SIMPLE_FILE_WATCHER_SRC_FILES ${simple_file_watcher_SOURCE_DIR}/source/FileWatcher.cpp
                                   ${simple_file_watcher_SOURCE_DIR}/source/FileWatcherLinux.cpp
                                   ${simple_file_watcher_SOURCE_DIR}/source/FileWatcherOSX.cpp
                                   ${simple_file_watcher_SOURCE_DIR}/source/FileWatcherWin32.cpp
)

if(NOT MSVC_COMPILER)
    set_source_files_properties( ${SIMPLE_FILE_WATCHER_SRC_FILES} PROPERTIES COMPILE_FLAGS "-Wno-switch-default" )
endif()

set( FCPP_SRC_FILES ${fcpp_SOURCE_DIR}/cpp1.c
                    ${fcpp_SOURCE_DIR}/cpp2.c
                    ${fcpp_SOURCE_DIR}/cpp3.c
                    ${fcpp_SOURCE_DIR}/cpp4.c
                    ${fcpp_SOURCE_DIR}/cpp5.c
                    ${fcpp_SOURCE_DIR}/cpp6.c
                    #${fcpp_SOURCE_DIR}/usecpp.c
)

if (NOT MSVC_COMPILER)
    set_source_files_properties( ${FCPP_SRC_FILES} PROPERTIES COMPILE_FLAGS "-Wno-switch-default -Wno-date-time -Wno-pedantic" )
endif()

set( IMGUIZMO_SRC_FILES ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
                        ${imguizmo_SOURCE_DIR}/GraphEditor.cpp
                        ${imguizmo_SOURCE_DIR}/ImCurveEdit.cpp
                        ${imguizmo_SOURCE_DIR}/ImGradient.cpp
                        ${imguizmo_SOURCE_DIR}/ImSequencer.cpp
)

set(IMGUIZMO_COMPILE_OPTIONS "")

if(MSVC_COMPILER)
    list(APPEND IMGUIZMO_COMPILE_OPTIONS "/wd4245 /wd4189")
else()
    list(APPEND IMGUIZMO_COMPILE_OPTIONS "-Wno-nested-anon-types -Wno-sign-compare -Wno-ignored-qualifiers -Wno-switch-default")
endif()
    
set_source_files_properties( ${IMGUIZMO_SRC_FILES} PROPERTIES COMPILE_FLAGS ${IMGUIZMO_COMPILE_OPTIONS} )

set( THIRD_PARTY_FETCH_SRC_FILES ${TILEABLE_VOLUME_NOISE_SRC_FILES}
                                 ${CURL_NOISE_SRC_FILES}
                                 ${SIMPLE_FILE_WATCHER_SRC_FILES}
                                 ${FCPP_SRC_FILES}
                                 ${IMGUIZMO_SRC_FILES}
)
