SET(SURVIVE_LIBRARY_TYPE SHARED)
if(BUILD_STATIC)
  SET(SURVIVE_LIBRARY_TYPE STATIC)
  add_definitions(-DSURVIVE_DISABLE_PLUGINS)
endif()

list (APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists (fopencookie stdio.h HAVE_FOPENCOOKIE)

if(HAVE_FOPENCOOKIE)
  add_definitions(-DHAVE_FOPENCOOKIE)
endif()


SET(SURVIVE_SRCS ${SURVIVE_SRCS}
  ootx_decoder.c
  poser.c
  poser_general_optimizer.c
  survive.c
  survive_api.c
        survive_config.c
  survive_default_devices.c
  survive_disambiguator.c
  survive_driverman.c
  survive_kalman_tracker.c
  survive_optimizer.c
  survive_recording.c        
  survive_plugins.c
        survive_process.c
  survive_process_gen2.c
  survive_sensor_activations.c
  survive_kalman.c
  barycentric_svd/barycentric_svd.c
  survive_reproject_gen2.c
        survive_process_gen1.c
        survive_reproject.c
  lfsr.c
  lfsr_lh2.c
  survive_str.h survive_str.c test_cases/str.c
  survive_async_optimizer.c
  ../redist/linmath.c ../redist/puff.c ../redist/symbol_enumerator.c
  ../redist/jsmn.c ../redist/json_helpers.c ../redist/crc32.c
  )

if(USE_HIDAPI OR WIN32)
  add_definitions (-DHIDAPI)
  IF(WIN32)
    SET(SURVIVE_SRCS ${SURVIVE_SRCS} ../redist/hid-windows.c ../winbuild/getdelim.c)
  else()
    list(APPEND ADDITIONAL_LIBRARIES udev hidapi-libusb)
  endif()
endif()

add_library(survive ${SURVIVE_LIBRARY_TYPE} ${SURVIVE_SRCS})

#find_package (Python3 COMPONENTS Interpreter Development)
#find_package(PythonExtensions REQUIRED)

#python_extension_module(survive)

set_target_properties(survive PROPERTIES
	FOLDER "libraries"
	SOVERSION ${PROJECT_VERSION_MAJOR}
	VERSION ${PROJECT_VERSION})
set_target_properties(survive PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(survive PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(WIN32)
    set_target_properties(survive PROPERTIES PREFIX "lib")
endif()

target_link_libraries(survive minimal_opencv ${ADDITIONAL_LIBRARIES} mpfit)
IF(HAVE_ZLIB_H)
  target_link_libraries(survive z)
ENDIF()

IF(Threads_FOUND)
  target_link_libraries(survive ${CMAKE_THREAD_LIBS_INIT} Threads::Threads)
endif()

if(NOT BUILD_STATIC)
  target_link_libraries(survive ${CMAKE_DL_LIBS})
endif()

IF(NOT WIN32)
	target_link_libraries(survive m )
else()
	set_target_properties( survive PROPERTIES VS_USER_PROPS "$(SolutionDir)\\packages\\OpenBLAS.0.2.14.1\\build\\native\\openblas.targets" )
	target_link_libraries(survive DbgHelp SetupAPI)
endif()


SET(PLUGINS
        driver_dummy driver_vive driver_simulator driver_playback
        disambiguator_statebased
        poser_dummy poser_mpfit poser_kalman_only
	poser_barycentric_svd driver_global_scene_solver
)

if(BUILD_GATT_SUPPORT)
	LIST(APPEND PLUGINS  driver_gatt)
	set(driver_gatt_ADDITIONAL_LIBS gattlib)
endif()

if(APPLE)
  execute_process(COMMAND brew --prefix libusb OUTPUT_VARIABLE LIBUSB_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
  include_directories(${LIBUSB_PREFIX}/include)
  link_directories(${LIBUSB_PREFIX}/lib)  
  message("Adding ${LIBUSB_PREFIX}/include to include directories")
endif()

IF(NOT WIN32)
  LIST(APPEND PLUGINS driver_udp)

  check_include_file(libusb.h LIBUSB_NO_DIR)
  check_include_file(libusb-1.0/libusb.h LIBUSB_VER)

  if(LIBUSB_NO_DIR)
    message("Using non-versioned libusb dir")
    add_definitions(-DSURVIVE_LIBUSB_NO_DIR)
  endif()
  if(LIBUSB_VER)
    message("Using versioned libusb dir")
    add_definitions(-DSURVIVE_LIBUSB_VER_DIR)
  endif()

  set(driver_vive_ADDITIONAL_LIBS usb-1.0)
  find_library(PCAP_LIBRARY pcap)
  if(PCAP_LIBRARY)
    list(APPEND PLUGINS driver_usbmon)
    set(driver_usbmon_ADDITIONAL_LIBS "${PCAP_LIBRARY};z" driver_vive)
  else()
    message("Can't build usbmon plugin -- pcap library was not found")
  endif()
endif()

set(poser_barycentric_svd_ADDITIONAL_SRCS barycentric_svd/barycentric_svd.c )

add_custom_target(survive_plugins)

if(OPENVR_LIBRARIES AND NOT USE_SINGLE_PRECISION)
	include_directories(redist include/libsurvive include ${OPENVR_INCLUDE_PATH})
	LIST(APPEND PLUGINS driver_openvr)
	set(driver_openvr_ADDITIONAL_LIBS ${OPENVR_LIBRARIES})
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(SURVIVE_BUILT_PLUGINS)

if(WIN32)
	SET(LIB_INSTALL_DIR "bin/${CMAKE_GENERATOR_PLATFORM}/" CACHE STRING "")
else()
	SET(LIB_INSTALL_DIR "lib/" CACHE STRING "")
endif()

function(SURVIVE_REGISTER_PLUGIN PLUGIN)
  IF(NOT TARGET ${PLUGIN})
    SET(SRC ${PLUGIN}.c)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
      SET(SRC ${PLUGIN}.cc)
    endif()
    add_library(${PLUGIN} ${SURVIVE_LIBRARY_TYPE} ${SRC} ${${PLUGIN}_ADDITIONAL_SRCS})
    target_link_libraries(${PLUGIN} survive ${${PLUGIN}_ADDITIONAL_LIBS})

    set_target_properties(${PLUGIN} PROPERTIES INSTALL_RPATH "$ORIGIN/../..;$ORIGIN")
  ENDIF()
  
  target_link_libraries(${PLUGIN} survive)
  list(APPEND SURVIVE_BUILT_PLUGINS ${PLUGIN})
  set(SURVIVE_BUILT_PLUGINS "${SURVIVE_BUILT_PLUGINS}" PARENT_SCOPE)

  add_dependencies(survive_plugins ${PLUGIN})
  STRING(REGEX REPLACE "(.*)\_.*" "\\1" plugin_type "${PLUGIN}")
  if(plugin_type)
    set_target_properties(${PLUGIN} PROPERTIES FOLDER "${plugin_type}")
  endif()

  set(OUTPUT_DIR "$<TARGET_FILE_DIR:survive>/")#set(OUTPUT_DIR "${survive_location}")

  if(NOT BUILD_STATIC)
    set_target_properties(${PLUGIN} PROPERTIES PREFIX "")
  endif()

  set(PLUGIN_SUFFIX "plugins")

  set_target_properties(${PLUGIN} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}${PLUGIN_SUFFIX}")
  set_target_properties(${PLUGIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}${PLUGIN_SUFFIX}")

  #set_target_properties(${PLUGIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}Debug/plugins")
  #set_target_properties(${PLUGIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}Release/plugins")
  #set_target_properties(${PLUGIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}RelWithDebInfo/plugins")
  install(TARGETS ${PLUGIN} DESTINATION ${LIB_INSTALL_DIR}libsurvive/${PLUGIN_SUFFIX})
endfunction()

foreach(PLUGIN ${PLUGINS})
  option(ENABLE_${PLUGIN} "Build ${PLUGIN}" ${NOT_CORE_BUILD})
  if(ENABLE_${PLUGIN})
    SURVIVE_REGISTER_PLUGIN(${PLUGIN})
  endif()
endforeach()

IF(TARGET CNGFX)
  list(APPEND SURVIVE_EXECUTABLES simple_pose_test)
  set(simple_pose_test_ADDITIONAL_LIBS CNGFX)
endif()

IF (ENABLE_TESTS)
  add_subdirectory(test_cases)
ENDIF()

set(SURVIVE_BUILT_PLUGINS "${SURVIVE_BUILT_PLUGINS}" PARENT_SCOPE)

install(TARGETS ${SURVIVE_BUILT_PLUGINS} DESTINATION ${LIB_INSTALL_DIR}libsurvive/plugins)
install(TARGETS survive DESTINATION ${LIB_INSTALL_DIR})

if(WIN32)
  cmake_policy(SET CMP0087 NEW)
  install(CODE "set(CMAKE_GENERATOR_PLATFORM \"${CMAKE_GENERATOR_PLATFORM}\")")
  install(CODE "set(LIB_INSTALL_DIR \"${LIB_INSTALL_DIR}\")")
  install(CODE [[  
  file(GET_RUNTIME_DEPENDENCIES LIBRARIES $<TARGET_FILE:survive>
    RESOLVED_DEPENDENCIES_VAR _r_deps
    UNRESOLVED_DEPENDENCIES_VAR _u_deps
	POST_EXCLUDE_REGEXES ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\][Ss]ystem32.*"
  )
  foreach(_file ${_r_deps})
    file(INSTALL
      DESTINATION "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}"
      TYPE SHARED_LIBRARY     
      FILES "${_file}"
    )
  endforeach()
]])
endif()
