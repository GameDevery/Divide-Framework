include(FetchContent)

if(NOT DEFINED BUILD_STATIC_CEGUI)
    set(BUILD_STATIC_CEGUI TRUE)
endif()

if(BUILD_STATIC_CEGUI)
    add_compile_definitions(CEGUI_BUILD_STATIC_FACTORY_MODULE)
endif()

FetchContent_Declare(
  cegui
  GIT_REPOSITORY https://github.com/cegui/cegui.git
  GIT_TAG e6c5f755efe499051fd59ad7edf0a6f83c475c5a
)

set(CEGUI_IMAGE_CODEC STBImageCodec)
set(CEGUI_IMAGE_CODEC_LIB "CEGUI${CEGUI_IMAGE_CODEC}")
set(CEGUI_OPTION_DEFAULT_IMAGECODEC ${CEGUI_IMAGE_CODEC})
set(CEGUI_STATIC_IMAGECODEC_MODULE ${CEGUI_IMAGE_CODEC_LIB})
set(CEGUI_BUILD_IMAGECODEC_STB TRUE)

set(CEGUI_XML_PARSER ExpatParser)
set(CEGUI_XML_PARSER_LIB "CEGUI${CEGUI_XML_PARSER}")
set(CEGUI_OPTION_DEFAULT_XMLPARSER ${CEGUI_XML_PARSER})
set(CEGUI_STATIC_XMLPARSER_MODULE ${CEGUI_XML_PARSER_LIB})
set(CEGUI_BUILD_XMLPARSER_EXPAT TRUE)

set(CEGUI_BUILD_STATIC_CONFIGURATION  ${BUILD_STATIC_CEGUI})
set(CEGUI_BUILD_STATIC_FACTORY_MODULE ${BUILD_STATIC_CEGUI})
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

FetchContent_MakeAvailable(cegui)

if(BUILD_STATIC_CEGUI)
    add_compile_definitions(CEGUI_STATIC)
endif()

set(CEGUI_LIBRARY_NAMES "CEGUIBase-0;CEGUICommonDialogs-0;CEGUICoreWindowRendererSet;${CEGUI_IMAGE_CODEC_LIB};${CEGUI_XML_PARSER_LIB}")

set(CEGUI_LIBRARIES "")

foreach(TARGET_LIB ${CEGUI_LIBRARY_NAMES})
    if(BUILD_STATIC_CEGUI)
        set(TARGET_LIB "${TARGET_LIB}_Static")
    endif()
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(TARGET_LIB "${TARGET_LIB}_d")
    endif()

    list(APPEND CEGUI_LIBRARIES ${TARGET_LIB})
endforeach()


include_directories(
    "${cegui_SOURCE_DIR}/cegui/include"
    "${cegui_BINARY_DIR}/cegui/include"
)