# -*- cmake -*-

project(lldarwin LANGUAGES CXX)

include(CMakeFindFrameworks)
include(00-Common)

find_library(COCOA_LIBRARY Cocoa)

set(lldarwin_SOURCE_FILES
    llappdelegate-objc.mm
    llappviewermacosx-objc.mm
    lldesktopnotifications-objc.mm
    llfilepicker-objc.mm
    llopenglview-objc.mm
    llsys-objc.mm
    llvfs-objc.mm
    llwindowmacosx-objc.mm
    )

set(lldarwin_HEADER_FILES
    CMakeLists.txt

    llappdelegate-objc.h
    llappviewermacosx-objc.h
    lldesktopnotifications-objc.h
    llfilepicker-objc.h
    llopenglview-objc.h
    llsys-objc.h
    llvfs-objc.h
    llwindowmacosx-objc.h
    )

set_source_files_properties(${lldarwin_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND lldarwin_SOURCE_FILES ${lldarwin_HEADER_FILES})

set_source_files_properties(llopenglview-objc.mm PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")

add_library (lldarwin ${lldarwin_SOURCE_FILES})

target_link_libraries(lldarwin
    ${COCOA_LIBRARIES}
    )
