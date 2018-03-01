cmake_minimum_required(VERSION 3.8)
project(libsurvive)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -flto -DUSE_DOUBLE -std=gnu99 -rdynamic -fsanitize=address -fsanitize=undefined")

include_directories(include/libsurvive redist)

add_library(libsurvive SHARED
  src/survive.c src/survive_usb.c src/survive_data.c src/survive_process.c src/ootx_decoder.c src/survive_driverman.c src/survive_vive.c src/survive_config.c src/survive_cal.c
  src/poser_dummy.c src/poser_daveortho.c src/poser_charlesslow.c src/poser_octavioradii.c src/poser_turveytori.c
  redist/json_helpers.c redist/linmath.c redist/jsmn.c redist/os_generic.c
  )

target_link_libraries(libsurvive pthread z m usb-1.0)

add_executable(pose_test test.c)
target_link_libraries(pose_test libsurvive)

add_executable(playback playback.c)
target_link_libraries(playback libsurvive)
