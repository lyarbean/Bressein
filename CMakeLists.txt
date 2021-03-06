project(Bressein)
cmake_minimum_required(VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(QT_USE_QTSQL TRUE )
set(QT_USE_QTXML TRUE )
set(QT_USE_QTGUI TRUE )
set(QT_USE_QTNETWORK TRUE )
set(QT_USE_QTDBUS TRUE )
# set(QT_USE_QTOPENGL TRUE )

# find_package(Doxygen REQUIRED)
find_package(Qt4 COMPONENTS QtSql QtXml QtGui QtNetwork REQUIRED)

include(${QT_USE_FILE} )
include_directories(${QT_INCLUDES}
                    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
#     add_definitions (-DQT_NO_DEBUG_OUTPUT)
# else (${CMAKE_BUILD_TYPE} STREQUAL "Release")
# endif(${CMAKE_BUILD_TYPE} STREQUAL "Release")

add_definitions(-std=c++0x -Wall)

qt4_add_resources(RC_SRCS gui/images.qrc )
add_subdirectory(sipc)
set(GUI_SRCS gui/groupitem.cpp gui/loginscene.cpp gui/loginwidget.cpp
    gui/bresseinmanager.cpp gui/textitem.cpp gui/chatview.cpp
    gui/sidepanelview.cpp gui/contactsscene.cpp gui/contactitem.cpp)
set(SRCS main.cpp ${GUI_SRCS})
qt4_automoc(${SRCS})

add_executable(bressein ${SRCS} ${RC_SRCS})
target_link_libraries(bressein bressein_sipc ssl ${QT_LIBRARIES})


## Doxygen
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES doc)
