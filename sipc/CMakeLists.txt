include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
set(SIPC_SRCS transporter.cpp storeman.cpp conversationmanager.cpp portraitfetcher.cpp account.cpp utils.cpp)
qt4_automoc(${SIPC_SRCS})
add_library(bressein_sipc STATIC ${SIPC_SRCS})

