# The complete Windows bundle is staged by packaging/build-windows.sh. CPack
# alone must not emit an executable-only ZIP missing Qt/Hamlib/audio helpers.
add_custom_target(windows-build-help
  COMMAND ${CMAKE_COMMAND} -E echo "Use Build Windows.cmd or packaging/build-windows.sh to build, test, deploy runtime libraries, and package the Windows preview.")
