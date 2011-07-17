set(SIPC_SRCS user.cpp)
qt4_automoc(${SIPC_SRCS})
add_library(sipc ${SIPC_SRCS})
link_libraries(${QT_LIBRARIES} ssl)
