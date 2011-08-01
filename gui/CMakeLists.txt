include_directories(${CMAKE_CURRENT_BINARY_DIR})
set(GUI_SRCS contactsscene.cpp contactitem.cpp)
qt4_automoc(${GUI_SRCS})
add_library(bressein_gui SHARED ${GUI_SRCS})
