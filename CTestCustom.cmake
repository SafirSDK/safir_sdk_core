if ($ENV{SAFIR_SKIP_SLOW_TESTS})
  MESSAGE("Warning: ENVIRONMENT VARIABLE SAFIR_SKIP_SLOW_TESTS IS SET! SKIPPING SOME TESTS!")
  set(CTEST_CUSTOM_TESTS_IGNORE
    LowLevelLogger
    DataSenderTest
    DiscovererTest
    ElectionHandler_test
    ElectionHandler_test_with_overflows
    LamportClocks
    dope_file_backend_test
    dope_none_backend_test
    restart_nodes
    StopHandler_test
    tracer_backdoor
    websocket_component_test
    websocket_stress_test
    Incarnation_And_Control_Tests
    )
endif()
