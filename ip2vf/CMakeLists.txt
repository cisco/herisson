cmake_minimum_required (VERSION 3.1)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project (vMI_all)

set(WANTED_PROBE_SUPPORT TRUE)


if (UNIX AND NOT APPLE)
   set (LINUX TRUE)
endif (UNIX AND NOT APPLE)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

#Detect if git repository or not
execute_process(COMMAND "git rev-parse" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE git_rv)
if(NOT "${git_rv}")
   message("This is a git repo")
   set (IS_GIT_REPO TRUE)
   add_definitions( -DIS_GIT_REPO )
endif()

if (WANTED_PROBE_SUPPORT)
   add_definitions( -DHAVE_PROBE )
   set(HAVE_PROBE TRUE)
endif()



#Detect OPENCV2
find_path (OPENCV2_INCLUDE_DIRS NAMES "opencv2/opencv.hpp" PATHS "../external/opencv/build/include/")
find_library (OPENCV2_LIBRARIES NAMES "opencv_world310.lib" PATHS "../external/opencv/build/x64/vc14/lib/")
if (OPENCV2_INCLUDE_DIRS AND OPENCV2_LIBRARIES)
   add_definitions( -DHAVE_OPENCV2 )
   set(HAVE_OPENCV2 TRUE)
endif()


#Detect X11
find_library(X11_LIBRARIES X11)
if (X11_LIBRARIES)
   add_definitions( -DHAVE_X11)
   set(HAVE_X11 TRUE)
endif()

#Detect ZLIB
find_path (ZLIB_INCLUDE_DIRS NAMES zlib.h PATHS "../external/zlib-1.2.3-dev/include/")
find_library (ZLIB_LIBRARIES NAMES z PATHS "../external/zlib-1.2.3-dev/lib/")
if (ZLIB_INCLUDE_DIRS AND ZLIB_LIBRARIES)
   add_definitions( -DHAVE_ZLIB )
   set(HAVE_ZLIB TRUE)
endif()

#Detect PNG
if (HAVE_ZLIB)
   find_path (PNG_INCLUDE_DIRS NAMES png.h PATHS "../external/libpng-1.2.37-dev/include/")
   find_library (PNG_LIBRARIES NAMES png libpng PATHS "../external/libpng-1.2.37-dev/lib/")
   if (PNG_INCLUDE_DIRS AND PNG_LIBRARIES)
      add_definitions( -DHAVE_PNG )
      set(HAVE_PNG TRUE)
   endif()
endif()


set(CMAKE_CXX_STANDARD 14)
set(COMMON_SOURCE_FILES
   "tools.cpp"
   "circularbuffer.cpp"
   "yuv.cpp"
   "log.cpp"
   "tcp_basic.cpp"
   "rtpframe.cpp"
   "frameheaders.cpp"
   "metricscollector.cpp"
   "collectdframe.cpp"
   "framecounter.cpp"
   "moduleconfiguration.cpp"
   "audiopacket.cpp"
   "vmiframe.cpp"
   "rtppacketizer.cpp"
   "rtpmmsgpacketizer.cpp"
   "hbrmppacketizer.cpp"
   "pins/st2022/datasource.cpp"
   "pins/st2022/datasourcefile.cpp"
   "pins/st2022/datasourceCachedFile.cpp"
   "pins/st2022/datasourceRTP.cpp"
   "pins/st2022/datasourceSPSRTP.cpp"
   "pins/st2022/datasourceRIO.cpp"
   "pins/st2022/hbrmpframe.cpp"
   "pins/st2022/smpteframe.cpp"
   "pins/st2022/smpteprofile.cpp"
   "pins/st2022/smptecrc.cpp"
   "pins/in.cpp"
   #"pins/infile.cpp"
   "pins/st2022/insmpte.cpp"
   "pins/shmem/inmem.cpp"
   "pins/rtp/inrtp.cpp"
   "pins/intcp.cpp"
   "pins/tr03/intr03.cpp"
   "pins/tr03/outtr03.cpp"
   "pins/aes67/inaes67.cpp"
   "pins/tr03/tr03frameparser.cpp"
   "pins/tr03/tr03frame.cpp"
   "pins/out.cpp"
   "pins/outdevnull.cpp"
   "pins/shmem/outmem.cpp"
   "pins/rtp/outrtp.cpp"
   "pins/st2022/outsmpte.cpp"
   "pins/outstorage.cpp"
   "pins/outtcp.cpp"
   "pins/outthumbsocket.cpp"
   "pins/pinfactory.cpp"
   "pins/vmistreamercisco2022_6.cpp"

   "pins/vmistreamerstorage.cpp"
)

foreach(f ${COMMON_SOURCE_FILES})
  list(APPEND COMMON_SOURCE_FILES_LIST "../common/${f}")
endforeach(f)

if (HAVE_PROBE)
    list(APPEND COMMON_SOURCE_FILES_LIST "../common/pins/pktTS.cpp")
endif()

add_subdirectory(libvMI)
add_subdirectory(vMIModules)
