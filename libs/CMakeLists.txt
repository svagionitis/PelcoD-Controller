add_subdirectory(PelcoDMath)
add_subdirectory(PelcoDCore)
add_subdirectory(PelcoDTransport)
add_subdirectory(PelcoDTracking)
add_subdirectory(PelcoDFujinon)
add_subdirectory(PelcoDSim)

if(PELCOD_ENABLE_ONVIF)
    add_subdirectory(PelcoDOnvif)
endif()

add_subdirectory(PelcoDQt)

if(PELCOD_ENABLE_VIDEO)
    add_subdirectory(PelcoDVideo)
    if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
        add_subdirectory(PelcoDVideoFilters)
    endif()
endif()
