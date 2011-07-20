project(Bressein)
cmake_minimum_required(VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE ON)
add_definitions( -Wall )
set( QT_USE_QTSQL TRUE )
set( QT_USE_QTXML TRUE )
set( QT_USE_QTCORE TRUE )
set( QT_USE_QTNETWORK TRUE )
find_package(Doxygen REQUIRED)
find_package(Qt4 REQUIRED)

include(${QT_USE_FILE} )
include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
#
# include(UseDoxygen OPTIONAL)


add_subdirectory(gui)
add_subdirectory(sipc)
add_subdirectory(utils)

set(Bressein_SRCS Bressein.cpp main.cpp)
qt4_automoc(${Bressein_SRCS})
add_executable(Bressein ${Bressein_SRCS})
target_link_libraries(Bressein sipc ssl ${QT_LIBRARIES})

###########doxygen###########
# prepare doxygen configuration file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# add doxygen as target
add_custom_target(doxygen ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# cleanup $build/api-doc on "make clean"
set_property(DIRECTORY APPEND PROPERTY
ADDITIONAL_MAKE_CLEAN_FILES doc)
