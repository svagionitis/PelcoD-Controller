add_subdirectory(PelcoDCore)
add_subdirectory(PelcoDQt)

if(PELCOD_ENABLE_VIDEO)
    add_subdirectory(PelcoDVideo)
endif()
