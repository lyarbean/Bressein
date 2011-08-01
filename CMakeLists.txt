project(Bressein)
cmake_minimum_required(VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(QT_USE_QTSQL TRUE )
set(QT_USE_QTXML TRUE )
set(QT_USE_QTCORE TRUE )
set(QT_USE_QTNETWORK TRUE )
find_package(Doxygen REQUIRED)
find_package(Qt4 REQUIRED)

include(${QT_USE_FILE} )
include_directories(${QT_INCLUDES}
     ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-std=c++0x -Wall)

add_subdirectory(sipc)
add_subdirectory(utils)
# add_subdirectory(gui)
set(GUI_SRCS gui/bressein.cpp gui/contactsscene.cpp gui/contactitem.cpp)
set(SRCS main.cpp ${GUI_SRCS})
qt4_automoc(${SRCS})
add_executable(bressein ${SRCS})
target_link_libraries(bressein bressein_sipc ssl ${QT_LIBRARIES})


## Doxygen
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES doc)
