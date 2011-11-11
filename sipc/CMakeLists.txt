include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
set(SIPC_SRCS messagerecorder.cpp transporter.cpp conversationmanager.cpp portraitfetcher.cpp
    account.cpp aux.cpp)
qt4_automoc(${SIPC_SRCS})
add_library(bressein_sipc STATIC ${SIPC_SRCS})
