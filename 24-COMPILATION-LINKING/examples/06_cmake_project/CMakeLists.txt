# ============================================================
# Ek chhota, "proper" CMake project — targets + properties.
# Lesson 12.
# ============================================================
cmake_minimum_required(VERSION 3.16)

project(cmake_demo
        VERSION 1.0.0
        DESCRIPTION "CPP-MASTERY folder 24 example"
        LANGUAGES CXX)

# ---- ek user-facing option (cmake -D se toggle) ----
option(ENGINE_FAST_PATH "Enable the fast code path" OFF)

# ---- library target ----
add_library(engine STATIC
    src/engine.cxx)

# PUBLIC  = ye include dir engine KO bhi chahiye AUR jo engine ko link kare use bhi
# PRIVATE = sirf engine ki apni build ke liye
# INTERFACE = sirf consumers ke liye (engine ki apni build ko nahi)
target_include_directories(engine PUBLIC include)

# C++20 — PUBLIC taaki consumers ko bhi mile
target_compile_features(engine PUBLIC cxx_std_20)

# option se compile definition
if(ENGINE_FAST_PATH)
    target_compile_definitions(engine PUBLIC ENGINE_FAST_PATH)
endif()

# generator expression: sirf Release config mein extra flags
target_compile_options(engine PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0>
    -Wall -Wextra)

# ---- executable target ----
add_executable(app src/main.cxx)

# app -> engine link. `engine` PUBLIC properties (include dir, cxx_std_20,
# ENGINE_FAST_PATH) app tak automatically propagate hote hain.
target_link_libraries(app PRIVATE engine)

# ---- install rules (cmake --install build --prefix <dir>) ----
include(GNUInstallDirs)
install(TARGETS app engine
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---- ek chhota status print ----
message(STATUS "ENGINE_FAST_PATH = ${ENGINE_FAST_PATH}")
message(STATUS "Build type       = ${CMAKE_BUILD_TYPE}")
