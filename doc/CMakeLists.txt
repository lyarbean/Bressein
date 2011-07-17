

# install HTML API documentation and manual pages
# set(DOC_PATH "share/doc/${CPACK_PACKAGE_NAME}-${VERSION}")
#
# install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/api-doc/html
# DESTINATION ${DOC_PATH}
# )

# install man pages into packages, scope is now project root..
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/api-doc/man/man3
DESTINATION share/man/man3/
)