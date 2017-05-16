project(AlchemyPlugin)

include(00-Common)
include(LLCommon)
include(LLPlugin)
include(Linking)
include(PluginAPI)
include(LLMessage)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
)
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    )

if (DARWIN)
    include(CMakeFindFrameworks)
    find_library(COCOA_LIBRARY Cocoa)
endif (DARWIN)


### AlchemyPlugin

set(AlchemyPlugin_SOURCE_FILES
    slplugin.cpp
    )

if (DARWIN)
  list(APPEND AlchemyPlugin_SOURCE_FILES
    slplugin-objc.mm
  )
  list(APPEND AlchemyPlugin_HEADER_FILES
    slplugin-objc.h
  )
endif (DARWIN)

set_source_files_properties(${AlchemyPlugin_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

if (AlchemyPlugin_HEADER_FILES)
  list(APPEND AlchemyPlugin_SOURCE_FILES ${AlchemyPlugin_HEADER_FILES})
endif (AlchemyPlugin_HEADER_FILES)

if (WINDOWS)
  list(APPEND AlchemyPlugin_SOURCE_FILES ${CMAKE_SOURCE_DIR}/tools/manifests/compatibility.manifest)
endif (WINDOWS)

add_executable(AlchemyPlugin
    WIN32
    MACOSX_BUNDLE
    ${AlchemyPlugin_SOURCE_FILES}
)

target_link_libraries(AlchemyPlugin
  ${LLPLUGIN_LIBRARIES}
  ${LLMESSAGE_LIBRARIES}
  ${LLCOMMON_LIBRARIES}
  ${PLUGIN_API_WINDOWS_LIBRARIES}
  ${PTHREAD_LIBRARY}
)
if (WINDOWS)
  target_link_libraries(AlchemyPlugin wer)
elseif (DARWIN)
  target_link_libraries(AlchemyPlugin ${COCOA_LIBRARY})

  set_target_properties(AlchemyPlugin
    PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/slplugin_info.plist
    )

  # Make sure the app bundle has a Resources directory (it will get populated by viewer-manifest.py later)
  add_custom_command(
    TARGET AlchemyPlugin POST_BUILD
    COMMAND mkdir
    ARGS
      -p
      ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/AlchemyPlugin.app/Contents/Resources
  )
endif (WINDOWS)

if (LL_TESTS)
  #ll_deploy_sharedlibs_command(AlchemyPlugin)
endif (LL_TESTS)
