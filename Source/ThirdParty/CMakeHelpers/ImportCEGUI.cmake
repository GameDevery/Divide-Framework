include(FetchContent)

set(CMAKE_CXX_FLAGS_OLD "${CMAKE_CXX_FLAGS}")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4312 /wd4477 /wd4996")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-return-type-c-linkage -Wno-int-to-pointer-cast -Wno-string-plus-int")
endif()


FetchContent_Declare(
  Cegui
  GIT_REPOSITORY https://github.com/IonutCava/cegui.git
  GIT_TAG origin/v0-8
  #GIT_PROGRESS   TRUE
  #SYSTEM
)

set(CEGUI_BUILD_STATIC_CONFIGURATION TRUE)
set(CEGUI_IMAGE_CODEC STBImageCodec)
set(CEGUI_IMAGE_CODEC_LIB "CEGUI${CEGUI_IMAGE_CODEC}")
set(CEGUI_BUILD_IMAGECODEC_STB TRUE)
set(CEGUI_XML_PARSER ExpatParser)
set(CEGUI_XML_PARSER_LIB "CEGUI${CEGUI_XML_PARSER}")
set(CEGUI_BUILD_XMLPARSER_EXPAT TRUE)
set(CEGUI_BUILD_STATIC_FACTORY_MODULE TRUE)
set(CEGUI_HAS_STD11_REGEX TRUE)
set(CEGUI_SAMPLES_ENABLED FALSE)
set(CEGUI_STRING_CLASS 1)
set(CEGUI_BUILD_APPLICATION_TEMPLATES FALSE)
set(CEGUI_FONT_USE_GLYPH_PAGE_LOAD TRUE)
set(CEGUI_BUILD_SHARED_LIBS_WITH_STATIC_DEPENDENCIES TRUE)
set(CEGUI_USE_FRIBIDI FALSE)
set(CEGUI_USE_MINIBIDI FALSE)
set(CEGUI_USE_GLEW FALSE)
set(CEGUI_USE_EPOXY FALSE)
set(CEGUI_BUILD_RENDERER_OPENGL FALSE)
set(CEGUI_BUILD_RENDERER_OPENGL3 FALSE)
set(CEGUI_BUILD_RENDERER_OPENGLES FALSE)
set(CEGUI_BUILD_RENDERER_OGRE FALSE)
set(CEGUI_BUILD_RENDERER_IRRLICHT FALSE)
set(CEGUI_BUILD_RENDERER_DIRECTFB FALSE)
set(CEGUI_BUILD_RENDERER_DIRECT3D11 FALSE)
set(CEGUI_BUILD_RENDERER_DIRECT3D10 FALSE)
set(CEGUI_BUILD_RENDERER_DIRECT3D9 FALSE)
set(CEGUI_BUILD_RENDERER_NULL FALSE)
set(CEGUI_BUILD_LUA_MODULE FALSE)
set(CEGUI_BUILD_LUA_GENERATOR FALSE)
set(CEGUI_BUILD_PYTHON_MODULES FALSE)
set(CEGUI_BUILD_SAFE_LUA_MODULE FALSE)
set(CEGUI_BUILD_XMLPARSER_EXPAT TRUE)

FetchContent_MakeAvailable(Cegui)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_OLD}")

set(CEGUI_LIBRARY_NAMES "CEGUIBase-0_Static;CEGUICommonDialogs-0_Static;CEGUICoreWindowRendererSet_Static;${CEGUI_IMAGE_CODEC_LIB}_Static;${CEGUI_XML_PARSER_LIB}_Static")

set(CEGUI_LIBRARIES "")

foreach(TARGET_LIB ${CEGUI_LIBRARY_NAMES})

    if (WIN32 AND (CMAKE_BUILD_TYPE MATCHES Debug))
        set(TARGET_LIB "${TARGET_LIB}_d")
    endif()

    list(APPEND CEGUI_LIBRARIES ${TARGET_LIB})
endforeach()

include_directories(
    "${cegui_SOURCE_DIR}/cegui/include"
    "${cegui_BINARY_DIR}/cegui/include"
)
link_directories("${cegui_BINARY_DIR}/lib" )
