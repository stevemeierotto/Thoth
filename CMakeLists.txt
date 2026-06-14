# Copyright (c) 2024 Steve Meierotto
# Thoth Control Panel
# Builds the Thoth GUI and the embedded basic_agent library.

cmake_minimum_required(VERSION 3.16)
project(Thoth VERSION 0.2 LANGUAGES CXX)

include(CTest)
enable_testing()

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Build type and debug flags ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer")

# --- Sanitizer options ---
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ENABLE_ASAN and ENABLE_TSAN cannot both be ON")
endif()

# --- Compiler warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)

    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()

    if(ENABLE_TSAN)
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endif()

# --- wxWidgets ---
find_package(wxWidgets REQUIRED COMPONENTS core base adv aui stc)
include(${wxWidgets_USE_FILE})

# --- CURL ---
find_package(CURL REQUIRED)

# --- Source files ---
set(SOURCES
    src/main.cpp
    src/MainFrame.cpp
    src/ChatSessionDataViewModel.cpp
    src/ChatSessionRenderer.cpp
    src/FileDropTarget.cpp
    src/AgentInterface.cpp
    src/VisualizationFrame.cpp
    src/ChatMessagePanel.cpp
    src/GragDiagnosticsPanel.cpp
    src/StrategyPanel.cpp
    src/PlanExecutionPanel.cpp
    src/TrajectoryViewer.cpp
    src/ExperimentLabPanel.cpp
    src/GraphPanel.cpp
    src/ExecutiveStateStrip.cpp
    src/BenchmarkWindow.cpp
)

# --- Add the external basic_agent project ---
add_subdirectory(external/basic_agent)

# --- Main executable ---
add_executable(thoth-control-panel ${SOURCES})

# --- Include directories ---
target_include_directories(thoth-control-panel PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${wxWidgets_INCLUDE_DIRS}
)

# --- Link libraries ---
target_link_libraries(thoth-control-panel
    PRIVATE
        ${wxWidgets_LIBRARIES}
        ${CURL_LIBRARIES}
        basic_agent
)

# --- Install target ---
install(TARGETS thoth-control-panel RUNTIME DESTINATION bin)

# --- Optional: Group IDE targets ---
set_target_properties(thoth-control-panel PROPERTIES FOLDER "apps")
set_target_properties(basic_agent PROPERTIES FOLDER "libs")

add_subdirectory(tests)

