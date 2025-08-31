# ScratchBird Source CMake Configuration
# Minimal configuration for planning phase

# Main executable (placeholder)
add_executable(scratchbird main.cpp)

# Set properties
set_target_properties(scratchbird PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Installation (for future use)
install(TARGETS scratchbird
    RUNTIME DESTINATION bin
)