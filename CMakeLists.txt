cmake_minimum_required(VERSION 3.5)
project(Gst)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pthread")
set(CMAKE_CXX_STANDARD 17)
## ------------------------------ Gstreamer -----------------------------------##
find_package(PkgConfig REQUIRED)
find_package(spdlog REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(
  ${GLIB_INCLUDE_DIRS}
)

link_directories(
  ${GLIB_LIBRARY_DIRS}
)

set(GSTREAMER_MINIMUM_VERSION 1.0.5)

pkg_check_modules(GST1_TEST gstreamer-1.0)
if ( GST1_TEST_FOUND AND NOT ${GST1_TEST_VERSION} VERSION_LESS ${GSTREAMER_MINIMUM_VERSION} )
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
    pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
    pkg_check_modules(GSTREAMER-APP REQUIRED gstreamer-app-1.0)
    pkg_check_modules(GSTREAMER-VIDEO REQUIRED gstreamer-video-1.0)
    pkg_check_modules(GSTREAMER-BASE REQUIRED gstreamer-base-1.0)
    pkg_check_modules(GSTREAMER-AUDIO REQUIRED gstreamer-audio-1.0)
    pkg_check_modules(GSTREAMER-PBUTILS REQUIRED gstreamer-pbutils-1.0)
    pkg_check_modules(GSTREAMER-FFT REQUIRED gstreamer-fft-1.0)
    add_definitions(-DGST_API_VERSION_1=1)
else()
    # fallback to gstreamer-0.10
    unset(GSTREAMER_MINIMUM_VERSION)
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-0.10)
    pkg_check_modules(GOBJECT REQUIRED gstreamer-0.10)
    pkg_check_modules(GSTREAMER-APP REQUIRED gstreamer-app-0.10)
    pkg_check_modules(GSTREAMER-VIDEO REQUIRED gstreamer-video-1.0)
    pkg_check_modules(GSTREAMER-BASE REQUIRED gstreamer-base-1.0)
    pkg_check_modules(GSTREAMER-AUDIO REQUIRED gstreamer-audio-0.10)
    pkg_check_modules(GSTREAMER-FFT REQUIRED gstreamer-fft-0.10)
    set_source_files_properties(WebKitWebAudioSourceGStreamer.cpp PROPERTIES COMPILE_DEFINITIONS "GLIB_DISABLE_DEPRECATION_WARNINGS=1")
endif()

include_directories(
  ${GSTREAMER_INCLUDE_DIRS}
  ${GOBJECT_INCLUDE_DIRS}
  ${GSTREAMER-APP_INCLUDE_DIRS}
  ${GSTREAMER-VIDEO_INCLUDE_DIRS}
  ${GSTREAMER-BASE_INCLUDE_DIRS}
  ${GSTREAMER-AUDIO_INCLUDE_DIRS}
  ${GSTREAMER-PBUTILS_INCLUDE_DIRS}
  ${GSTREAMER-FFT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

link_directories(
  ${GSTREAMER_LIBRARY_DIRS}
${GOBJECT_LIBRARY_DIRS}
  ${GSTREAMER-APP_LIBRARY_DIRS}
  ${GSTREAMER-VIDEO_LIBRARY_DIRS}
  ${GSTREAMER-BASE_LIBRARY_DIRS}
  ${GSTREAMER-AUDIO_LIBRARY_DIRS}
  ${GSTREAMER-PBUTILS_LIBRARY_DIRS}
  ${GSTREAMER-FFT_LIBRARY_DIRS}
)

set(gst_SOURCES
  GStreamerUtilities.cpp
  GOwnPtr.cpp
  GRefPtr.cpp
  AudioStreamChannelsReader.cpp
)


set(gst_LIBRARIES
  ${GSTREAMER_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER-APP_LIBRARIES}
  ${GSTREAMER-VIDEO_LIBRARIES}
  ${GSTREAMER-BASE_LIBRARIES}
  ${GSTREAMER-AUDIO_LIBRARIES}
  ${GSTREAMER-PBUTILS_LIBRARIES}
  ${GSTREAMER-FFT_LIBRARIES}
    
  pthread
  ${GLIB_LIBRARIES}
  ${GLIB_GIO_LIBRARIES}
  ${GLIB_GOBJECT_LIBRARIES}
)
find_package(spdlog REQUIRED)
include_directories(${PROJECT_SOURCE_DIR})

file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/main.cpp
		  ${PROJECT_SOURCE_DIR}/SplitMuxSink.cpp
		  ${PROJECT_SOURCE_DIR}/DirTool.cpp)


add_executable(${PROJECT_NAME} ${SOURCES})
##------------------ Gstreamer libraries -------------
target_link_libraries(${PROJECT_NAME} ${gst_LIBRARIES})

##----------------- OpenCV ---------------------------
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio imgcodecs)
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})
target_link_libraries(${PROJECT_NAME} spdlog::spdlog)
