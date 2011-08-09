include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
set(SIPC_SRCS storeman.cpp conversationmanager.cpp portraitfetcher.cpp user.cpp utils.cpp)
qt4_automoc(${SIPC_SRCS})
add_library(bressein_sipc STATIC ${SIPC_SRCS})

