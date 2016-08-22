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

add_executable(AlchemyPlugin
    WIN32
    MACOSX_BUNDLE
    ${AlchemyPlugin_SOURCE_FILES}
)

if (WINDOWS)
set_target_properties(AlchemyPlugin
  PROPERTIES
  LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
  LINK_FLAGS_DEBUG "/NODEFAULTLIB:\"LIBCMT;LIBCMTD;MSVCRT\""
  )
  
  add_custom_command(
    TARGET AlchemyPlugin POST_BUILD
    COMMAND "mt.exe"
    ARGS
    -manifest \"${CMAKE_SOURCE_DIR}\\tools\\manifests\\compatibility.manifest\" 
    -inputresource:\"$<TARGET_FILE:AlchemyPlugin>\"\;\#1 
    -outputresource:\"$<TARGET_FILE:AlchemyPlugin>\"\;\#1
    COMMENT "Adding compatibility manifest to AlchemyPlugin..." 
    )
else ()
set_target_properties(AlchemyPlugin
  PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/slplugin_info.plist
  )
endif ()

target_link_libraries(AlchemyPlugin
  ${LLPLUGIN_LIBRARIES}
  ${LLMESSAGE_LIBRARIES}
  ${LLCOMMON_LIBRARIES}
  ${PLUGIN_API_WINDOWS_LIBRARIES}
  ${PTHREAD_LIBRARY}
)

if (DARWIN)
  target_link_libraries(AlchemyPlugin ${COCOA_LIBRARY})
  # Make sure the app bundle has a Resources directory (it will get populated by viewer-manifest.py later)
  add_custom_command(
    TARGET AlchemyPlugin POST_BUILD
    COMMAND mkdir
    ARGS
      -p
      ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/AlchemyPlugin.app/Contents/Resources
  )
endif (DARWIN)

if (LL_TESTS)
  ll_deploy_sharedlibs_command(AlchemyPlugin)
endif (LL_TESTS)
