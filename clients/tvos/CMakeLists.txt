cmake_minimum_required(VERSION 3.20)
project(LineClash LANGUAGES C OBJC)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "tvOS")
  message(FATAL_ERROR "Use scripts/build-tvos.sh to select a tvOS SDK")
endif()
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
file(STRINGS "${ROOT}/version.mk" VERSION_LINE REGEX "^VERSION[ ?:=]+")
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" LC_VERSION "${VERSION_LINE}")
find_package(SDL2 REQUIRED CONFIG)
set(GUI "${ROOT}/clients/gui/src")
execute_process(COMMAND sh "${ROOT}/get-version.sh" header "${GUI}/buildnum.h" WORKING_DIRECTORY "${ROOT}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(LineClash MACOSX_BUNDLE
  "${GUI}/lineclash.c" "${GUI}/client.c" "${GUI}/directory.c" "${GUI}/accounts.c"
  "${GUI}/account_https.c" "${GUI}/render_sdl.c" "${GUI}/sound.c" "${GUI}/music.c"
  "${GUI}/music_decoder.c" "${GUI}/mod.c" platform.m http.m focus.c
  "${ROOT}/shared/src/field.c" "${ROOT}/shared/src/piece.c" "${ROOT}/shared/src/net.c" "${ROOT}/shared/src/protocol.c")
set_source_files_properties("${GUI}/lineclash.c" PROPERTIES COMPILE_DEFINITIONS "main=SDL_main")
set_source_files_properties(platform.m http.m PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_compile_features(LineClash PRIVATE c_std_11)
target_compile_definitions(LineClash PRIVATE PLATFORM_POSIX PLATFORM_TVOS LINECLASH_VERSION="${LC_VERSION}" TETRINETZ_VERSION="${LC_VERSION}")
target_include_directories(LineClash PRIVATE "${GUI}" "${ROOT}/shared/include" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(LineClash PRIVATE SDL2::SDL2main SDL2::SDL2 "-framework Foundation" "-framework UIKit")
target_link_options(LineClash PRIVATE -ObjC)
set_target_properties(LineClash PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER se.kladhest.lineclash.tv
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY 3
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
add_custom_command(TARGET LineClash POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${ROOT}/music" "$<TARGET_BUNDLE_DIR:LineClash>/music"
  COMMAND ${CMAKE_COMMAND} -E copy "${ROOT}/CREDITS.md" "${ROOT}/LICENSE.md" "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" "$<TARGET_BUNDLE_DIR:LineClash>/")
