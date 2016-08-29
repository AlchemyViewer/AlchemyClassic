# -*- cmake -*-

project(media_plugin_base)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLPlugin)
include(LLMath)
include(LLRender)
include(LLWindow)
include(Linking)
include(PluginAPI)
include(OpenGL)

### media_plugin_base

set(media_plugin_base_SOURCE_FILES
    media_plugin_base.cpp
)

set(media_plugin_base_HEADER_FILES
    CMakeLists.txt

    media_plugin_base.h
)

add_library(media_plugin_base
    ${media_plugin_base_SOURCE_FILES}
)

set_target_properties(media_plugin_base PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(
    media_plugin_base
    ${LLPLUGIN_LIBRARIES}
    ${LLCOMMON_LIBRARIES}
    )

