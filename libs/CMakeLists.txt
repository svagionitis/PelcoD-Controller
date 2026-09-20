add_subdirectory(PelcoDMath)
add_subdirectory(PelcoDCore)
add_subdirectory(PelcoDTransport)

if(PELCOD_ENABLE_ONVIF)
    add_subdirectory(PelcoDOnvif)
endif()

add_subdirectory(PelcoDQt)

if(PELCOD_ENABLE_VIDEO)
    add_subdirectory(PelcoDVideo)
endif()
