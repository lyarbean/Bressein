project(Bressein)
cmake_minimum_required(VERSION 2.6)
find_package(Qt4 REQUIRED)

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(Bressein_SRCS Bressein.cpp main.cpp)
qt4_automoc(${Bressein_SRCS})
add_executable(Bressein ${Bressein_SRCS})
target_link_libraries(Bressein ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY})

add_subdirectory(gui)
add_subdirectory(sipc)