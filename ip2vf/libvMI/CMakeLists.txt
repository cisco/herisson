set(LIBVMI_SOURCE_FILES_LIST 
   "libvMI.cpp"
   "vMI_input.cpp"
   "vMI_output.cpp"
   "vMI_module.cpp")
   
if (IS_GIT_REPO)
   execute_process( COMMAND git log -1 "--pretty='%h - %an, %ar : %s \t'" OUTPUT_VARIABLE GIT_LAST_10_COMMITS )
   execute_process( COMMAND git rev-parse --abbrev-ref HEAD OUTPUT_VARIABLE GIT_CURRENTBRANCH)
   string(REGEX REPLACE "\n" " " GIT_LAST_10_COMMITS "${GIT_LAST_10_COMMITS}")
   string(REGEX REPLACE "\n" " " GIT_CURRENTBRANCH "${GIT_CURRENTBRANCH}")
   file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gitversion.cpp" "const char *gitlast10commits = \"${GIT_LAST_10_COMMITS}\";\n")
   file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/gitversion.cpp" "const char *gitcurrentbranch = \"${GIT_CURRENTBRANCH}\";\n")
   set(GIT_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/gitversion.cpp")
else()
   set(GIT_VERSION_FILE "")
endif()



add_library(vMI SHARED ${COMMON_SOURCE_FILES_LIST} ${LIBVMI_SOURCE_FILES_LIST} "${GIT_VERSION_FILE}")
add_library(vMI_static STATIC ${COMMON_SOURCE_FILES_LIST} ${LIBVMI_SOURCE_FILES_LIST} "${GIT_VERSION_FILE}")

add_definitions( -DVMILIBRARY_EXPORTS )

if(HAVE_PNG)
   target_link_libraries(vMI PRIVATE "${PNG_LIBRARIES}")
   target_link_libraries(vMI_static PRIVATE "${PNG_LIBRARIES}")
   target_include_directories(vMI PUBLIC "${PNG_INCLUDE_DIRS}")
   target_include_directories(vMI_static PUBLIC "${PNG_INCLUDE_DIRS}")
endif()

if(WIN32)
   target_link_libraries(vMI PRIVATE wsock32 ws2_32)
   target_link_libraries(vMI_static PRIVATE wsock32 ws2_32)
endif()

if(LINUX)
   set(THREADS_PREFER_PTHREAD_FLAG ON)
   find_package(Threads REQUIRED)
   target_link_libraries(vMI PRIVATE ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(vMI PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()

target_include_directories(vMI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common/")
target_include_directories(vMI_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common/")

