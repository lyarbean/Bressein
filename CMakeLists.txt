project(Bressein)
cmake_minimum_required(VERSION 2.6)
add_definitions( -Wall )
set( QT_USE_QTSQL TRUE )
set( QT_USE_QTXML TRUE )
set( QT_USE_QTCORE TRUE )
set( QT_USE_QTNETWORK TRUE )
find_package(Qt4 REQUIRED)
INCLUDE(${QT_USE_FILE} )
include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
# check if doxygen is even installed
find_package(Doxygen)
if (DOXYGEN_FOUND STREQUAL "NO")
    message(FATAL_ERROR "Doxygen not found. Please get a copy http://www.doxygen.org")
endif (DOXYGEN_FOUND STREQUAL "NO")

set(Bressein_SRCS Bressein.cpp main.cpp)
qt4_automoc(${Bressein_SRCS})
add_executable(Bressein ${Bressein_SRCS})
target_link_libraries(Bressein ${QT_LIBRARIES} ssl)

add_subdirectory(gui)
add_subdirectory(sipc)
#add_subdirectory(doc)


# prepare doxygen configuration file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# add doxygen as target
add_custom_target(doxygen ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# cleanup $build/api-doc on "make clean"
set_property(DIRECTORY APPEND PROPERTY
ADDITIONAL_MAKE_CLEAN_FILES api-doc)

# add doxygen as dependency to doc-target
get_target_property(DOC_TARGET doc TYPE)
if(NOT DOC_TARGET)
    add_custom_target(doc)
endif()
add_dependencies(doc doxygen)