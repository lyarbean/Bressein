include_directories(${CMAKE_CURRENT_BINARY_DIR})
set(SIPC_SRCS user.cpp utils.cpp)
qt4_automoc(${SIPC_SRCS})
add_library(bressein_sipc STATIC ${SIPC_SRCS})

