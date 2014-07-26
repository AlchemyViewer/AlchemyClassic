# -*- cmake -*-
include(Prebuilt)
include(LibXML2)

if (USESYSTEMLIBS)
  include(FindPkgConfig)

  pkg_check_modules(GSTREAMER010 REQUIRED gstreamer-0.10)
  pkg_check_modules(GSTREAMER010_PLUGINS_BASE REQUIRED gstreamer-plugins-base-0.10)
elseif (LINUX)
  use_prebuilt_binary(gstreamer)
  set(GSTREAMER010_FOUND ON FORCE BOOL)
  set(GSTREAMER010_PLUGINS_BASE_FOUND ON FORCE BOOL)
  set(GSTREAMER010_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include/gstreamer-0.10
      ${LIBS_PREBUILT_DIR}/include/glib-2.0
      ${LIBXML2_INCLUDES}
      )
  # We don't need to explicitly link against gstreamer itself, because
  # LLMediaImplGStreamer probes for the system's copy at runtime.
  set(GSTREAMER010_LIBRARIES
      gobject-2.0
      gmodule-2.0
      dl
      gthread-2.0
      glib-2.0
      )
endif (USESYSTEMLIBS)

if (GSTREAMER010_FOUND AND GSTREAMER010_PLUGINS_BASE_FOUND)
  set(GSTREAMER010 ON CACHE BOOL "Build with GStreamer-0.10 streaming media support.")
endif (GSTREAMER010_FOUND AND GSTREAMER010_PLUGINS_BASE_FOUND)

if (GSTREAMER010)
  add_definitions(-DLL_GSTREAMER010_ENABLED=1)
endif (GSTREAMER010)

