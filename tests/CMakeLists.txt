add_executable(run_test_suite run_test_suite.cpp)
target_include_directories(run_test_suite PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
)
target_link_libraries(run_test_suite PRIVATE basic_agent)

add_test(NAME test-suite-dev COMMAND run_test_suite --dev)
set_tests_properties(test-suite-dev PROPERTIES
    LABELS "pr;fast;cognitive"
    TIMEOUT 180
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Core unit tests — no wxWidgets; always built when tests/ is in the graph.
find_package(Threads REQUIRED)

# Opt-in E2E reliability harness. Intentionally not registered with add_test,
# so default CTest runs do not execute it.
add_executable(thoth-reliability-harness reliability_harness.cpp)
target_compile_definitions(thoth-reliability-harness PRIVATE
    THOTH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_include_directories(thoth-reliability-harness PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-reliability-harness PRIVATE basic_agent Threads::Threads)

# Opt-in graph-recovery process campaign. Not registered with CTest.
add_executable(thoth-graph-recovery-e2e graph_recovery_e2e.cpp)
target_compile_definitions(thoth-graph-recovery-e2e PRIVATE
    THOTH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_include_directories(thoth-graph-recovery-e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-graph-recovery-e2e PRIVATE basic_agent sqlite3 Threads::Threads)

# Phase 10C-1 C6 metrics recovery. Isolated stores only.
add_executable(thoth-c6-metrics-recovery-tests c6_metrics_recovery_tests.cpp)
target_include_directories(thoth-c6-metrics-recovery-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-c6-metrics-recovery-tests PRIVATE basic_agent Threads::Threads)
add_test(NAME thoth-c6-metrics-recovery-tests COMMAND thoth-c6-metrics-recovery-tests)
set_tests_properties(thoth-c6-metrics-recovery-tests PROPERTIES
    LABELS "cognitive"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Phase 10C-2 episode checkpoint recovery. Isolated stores only.
add_executable(thoth-episode-recovery-tests episode_recovery_tests.cpp)
target_include_directories(thoth-episode-recovery-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-episode-recovery-tests PRIVATE basic_agent sqlite3 Threads::Threads)
add_test(NAME thoth-episode-recovery-tests COMMAND thoth-episode-recovery-tests)
set_tests_properties(thoth-episode-recovery-tests PROPERTIES
    LABELS "cognitive"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Phase 10C-3 reflection recovery. Isolated stores only.
add_executable(thoth-reflection-recovery-tests reflection_recovery_tests.cpp)
target_include_directories(thoth-reflection-recovery-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-reflection-recovery-tests PRIVATE basic_agent Threads::Threads)
add_test(NAME thoth-reflection-recovery-tests COMMAND thoth-reflection-recovery-tests)
set_tests_properties(thoth-reflection-recovery-tests PROPERTIES
    LABELS "cognitive"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Phase 10F-2 durable cognitive metrics outbox. Isolated stores only.
add_executable(thoth-cognitive-metrics-outbox-tests cognitive_metrics_outbox_tests.cpp)
target_include_directories(thoth-cognitive-metrics-outbox-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-cognitive-metrics-outbox-tests PRIVATE basic_agent sqlite3 Threads::Threads)
add_test(NAME thoth-cognitive-metrics-outbox-tests COMMAND thoth-cognitive-metrics-outbox-tests)
set_tests_properties(thoth-cognitive-metrics-outbox-tests PROPERTIES
    LABELS "cognitive"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Opt-in EGAR experience-path fixture. Not an experiment arm and not registered with CTest.
add_executable(thoth-egar-experience-e2e egar_experience_e2e.cpp)
target_include_directories(thoth-egar-experience-e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-egar-experience-e2e PRIVATE basic_agent sqlite3 Threads::Threads)

# Opt-in live EGAR lifecycle fixture. Not an experiment arm and not registered with CTest.
add_executable(thoth-egar-live-lifecycle egar_live_lifecycle.cpp)
target_include_directories(thoth-egar-live-lifecycle PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-egar-live-lifecycle PRIVATE basic_agent sqlite3 Threads::Threads)

add_executable(thoth-core-tests unit_tests.cpp)
target_compile_definitions(thoth-core-tests PRIVATE THOTH_HAS_GUI=0)
target_include_directories(thoth-core-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-core-tests PRIVATE basic_agent alp_migration sqlite3 Threads::Threads)
add_test(NAME thoth-core-tests COMMAND thoth-core-tests)
set_tests_properties(thoth-core-tests PROPERTIES
    LABELS "pr;unit"
    TIMEOUT 1800
)

add_executable(thoth-cm1-tests cm1_telemetry_tests.cpp)
target_include_directories(thoth-cm1-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-cm1-tests PRIVATE basic_agent sqlite3 Threads::Threads)
add_test(NAME thoth-cm1-tests COMMAND thoth-cm1-tests)
set_tests_properties(thoth-cm1-tests PROPERTIES
    LABELS "pr;unit;cm1"
    TIMEOUT 120
)

add_executable(thoth-cm1-twin-equivalence cm1_twin_equivalence.cpp)
target_include_directories(thoth-cm1-twin-equivalence PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-cm1-twin-equivalence PRIVATE basic_agent sqlite3 Threads::Threads)
# Opt-in verification harness. Not an official EGAR experiment gate.
add_test(NAME thoth-cm1-twin-equivalence COMMAND thoth-cm1-twin-equivalence)
set_tests_properties(thoth-cm1-twin-equivalence PROPERTIES
    LABELS "cm1;verification"
    TIMEOUT 180
)

# Live evidence-judgment benchmark. Not registered with CTest: it calls the
# local model and is not an EGAR, graph, or longitudinal gate.
add_executable(thoth-cm1-live-evidence cm1_live_evidence.cpp)
target_include_directories(thoth-cm1-live-evidence PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-cm1-live-evidence PRIVATE basic_agent sqlite3 Threads::Threads)

# Live controller E2E for the single-passage baseline and the two-passage
# scenario. Not registered with CTest. It calls the local model. Run only
# with an explicit operator request.
add_executable(thoth-cm1-live-controller cm1_live_controller.cpp)
target_include_directories(thoth-cm1-live-controller PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-cm1-live-controller PRIVATE basic_agent sqlite3 Threads::Threads)

# EGAR-LX Package 1. Each case is a new process so the containment flag starts off.
add_executable(thoth-containment-tests containment_tests.cpp)
target_include_directories(thoth-containment-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
)
target_link_libraries(thoth-containment-tests PRIVATE basic_agent Threads::Threads)
set(THOTH_CONTAINMENT_CASES
    chat_web_scrape
    chat_code_modify
    workflow_tool_retry
    retrieval_payload_tool
    retrieval_payload_tool_name
    retrieval_step_tool
    retrieval_empty_and_nonstr
    ordinary_retrieval
    resumed_tool_step
    confirm_status
    no_confirmation
    confirmed_true
    disabled_behavior
    step_failed_metadata
)
foreach(case IN LISTS THOTH_CONTAINMENT_CASES)
    add_test(NAME containment-${case} COMMAND thoth-containment-tests ${case})
    set_tests_properties(containment-${case} PROPERTIES
        LABELS "pr;unit;containment"
        TIMEOUT 120
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()

if(THOTH_BUILD_GUI)
    add_executable(thoth-gui-tests
        unit_tests.cpp
        ${CMAKE_SOURCE_DIR}/src/AgentInterface.cpp
        ${CMAKE_SOURCE_DIR}/src/local_agent_backend.cpp
        ${CMAKE_SOURCE_DIR}/src/remote_agent_backend.cpp
    )
    target_compile_definitions(thoth-gui-tests PRIVATE
        THOTH_HAS_GUI=1
        THOTH_GUI_TESTS_ONLY=1
    )
    target_include_directories(thoth-gui-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/includes
        ${CMAKE_SOURCE_DIR}/external/basic_agent/include
        ${CMAKE_SOURCE_DIR}/external/basic_agent/third_party
        ${wxWidgets_INCLUDE_DIRS}
    )
    target_link_libraries(thoth-gui-tests PRIVATE
        basic_agent
        alp_migration
        ${wxWidgets_LIBRARIES}
        ${CURL_LIBRARIES}
    )
    add_test(NAME thoth-gui-tests COMMAND thoth-gui-tests)
    set_tests_properties(thoth-gui-tests PROPERTIES
        LABELS "gui"
        TIMEOUT 300
    )
endif()

# Build-level compatibility alias — not a CTest entry. Run tests via
# ctest -R thoth-core-tests (and ctest -L gui for GUI tests).
add_custom_target(thoth-unit-tests
    DEPENDS thoth-core-tests
    COMMENT "Build alias — run ctest -R thoth-core-tests; GUI: ctest -L gui"
)
if(THOTH_BUILD_GUI)
    add_dependencies(thoth-unit-tests thoth-gui-tests)
endif()

if(TARGET run_reflection_ab_benchmark)
    add_test(NAME reflection-ab-benchmark COMMAND run_reflection_ab_benchmark)
    set_tests_properties(reflection-ab-benchmark PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 120
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(TARGET run_robustness_suite)
    add_test(NAME robustness-suite COMMAND run_robustness_suite)
    set_tests_properties(robustness-suite PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(TARGET run_trajectory_ablation_benchmark)
    add_test(NAME trajectory-ablation-benchmark COMMAND run_trajectory_ablation_benchmark --sample 2)
    set_tests_properties(trajectory-ablation-benchmark PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "THOTH_TRAJECTORY_ABLATION_TFIDF=1"
    )
endif()

if(TARGET run_episodic_learning_benchmark)
    add_test(NAME episodic-learning-benchmark COMMAND run_episodic_learning_benchmark)
    set_tests_properties(episodic-learning-benchmark PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

add_test(NAME c64-longitudinal-analyzer COMMAND python3 scripts/test_c64_longitudinal_analyzer.py)
set_tests_properties(c64-longitudinal-analyzer PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c64-window-attribution COMMAND python3 scripts/test_c64_window.py)
set_tests_properties(c64-window-attribution PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c64-environment-identity COMMAND python3 scripts/test_c64_environment_identity.py)
set_tests_properties(c64-environment-identity PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c6-longitudinal-join COMMAND python3 scripts/test_c6_longitudinal_join.py)
set_tests_properties(c6-longitudinal-join PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c6-longitudinal-analyzer COMMAND python3 scripts/test_c6_longitudinal_analyzer.py)
set_tests_properties(c6-longitudinal-analyzer PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c6-longitudinal-reporting COMMAND python3 scripts/test_c6_longitudinal_reporting.py)
set_tests_properties(c6-longitudinal-reporting PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

option(THOTH_TEST_SUITE_FULL "Register nightly Ollama TEST_SUITE (--full) in CTest" OFF)
if(THOTH_TEST_SUITE_FULL)
    add_test(NAME test-suite-full COMMAND run_test_suite --full)
    set_tests_properties(test-suite-full PROPERTIES
        LABELS "nightly;ollama;cognitive"
        TIMEOUT 3600
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
