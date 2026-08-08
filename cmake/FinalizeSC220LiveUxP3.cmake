# Final hardware-test authority for SC220 Live P3.
# Keep these passes last in the generator chain so no older source transformer can
# silently restore hidden PC capture, linked-only Dual Mono, Settings-only noise
# controls, or an LUFS-I value presented as a live loudness meter.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P3 finalizer requires engine, UI and app sources")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/SC220P3Routing.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/SC220P3InputUx.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/SC220P3InputUxPost.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/SC220P3Loudness.cmake")

message(STATUS "SC220 Live P3 final authority generated")
