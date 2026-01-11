find_package(Catch2 CONFIG REQUIRED)

#################################
### Test Configuration List  ###
#################################

# Single source of truth for all unit tests
# Just add the source file path - test name is derived from filename
set(UNIT_TESTS
    huira/math/test_rotation.cpp

    huira/units/test_units.cpp
)

#############################
### Coverage Configuration ###
#############################

option(HUIRA_COVERAGE "Enable code coverage reporting" OFF)

if(HUIRA_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC coverage flags
        set(COVERAGE_FLAGS --coverage -fprofile-arcs -ftest-coverage -g)
        message(STATUS "Code coverage enabled (GCC)")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(WIN32)
            # Clang on Windows - use source-based coverage
            set(COVERAGE_FLAGS -fprofile-instr-generate -fcoverage-mapping -g)
            message(STATUS "Code coverage enabled (Clang/Windows)")
        else()
            # Clang on Unix - can use GCC-compatible flags or source-based
            set(COVERAGE_FLAGS --coverage -g)
            message(STATUS "Code coverage enabled (Clang/Unix)")
        endif()
    else()
        message(WARNING "Code coverage not supported for ${CMAKE_CXX_COMPILER_ID}")
        set(HUIRA_COVERAGE OFF)
    endif()
endif()

###############################
### Test Creation Function ###
###############################

function(add_unit_test test_name source_file)
    add_executable(${test_name} ${source_file})
    
    target_link_libraries(${test_name} 
        PRIVATE 
        huira::huira
        Catch2::Catch2
        Catch2::Catch2WithMain
    )
    
    # Add coverage flags if enabled
    if(HUIRA_COVERAGE)
        target_compile_options(${test_name} PRIVATE ${COVERAGE_FLAGS})
        
        # Only add linker flags for non-Windows Clang or GCC
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR 
           (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32))
            target_link_libraries(${test_name} PRIVATE --coverage)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
            # The runtime will be automatically linked
        endif()
        
    endif()
    
    # Disable the problematic warning for MSVC
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /wd4868)
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test labels for filtering
    set_tests_properties(${test_name} PROPERTIES LABELS "unit")
    
    # Add to a target group for convenience
    if(TARGET all_unit_tests)
        add_dependencies(all_unit_tests ${test_name})
    else()
        add_custom_target(all_unit_tests DEPENDS ${test_name})
    endif()
endfunction()

############################
### Create All Unit Tests ###
############################

foreach(source_file ${UNIT_TESTS})
    # Extract test name from filename (without extension)
    get_filename_component(test_name ${source_file} NAME_WE)
    
    # Create the test
    add_unit_test(${test_name} ${source_file})
endforeach()

############################
### CI Integration Setup ###
############################

# Configure Catch2 to output JUnit XML
set(CATCH2_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test-results")
file(MAKE_DIRECTORY ${CATCH2_OUTPUT_DIR})

function(configure_test_output test_name)
    get_test_property(${test_name} COMMAND current_command)
    set_tests_properties(${test_name} PROPERTIES 
        COMMAND "${current_command};-r;junit;-o;${CATCH2_OUTPUT_DIR}/${test_name}.xml"
    )
endfunction()
# Apply to all tests
foreach(source_file ${UNIT_TESTS})
    get_filename_component(test_name ${source_file} NAME_WE)
    configure_test_output(${test_name})
endforeach()

###########################
### Coverage Target Setup ###
###########################

if(HUIRA_COVERAGE)
    set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage")
    file(MAKE_DIRECTORY ${COVERAGE_OUTPUT_DIR})
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(LCOV_EXECUTABLE lcov)
        find_program(GENHTML_EXECUTABLE genhtml)
        
        if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
            # Build command to run each test with XML output
            set(RUN_TESTS_COMMAND "")
            foreach(source_file ${UNIT_TESTS})
                get_filename_component(test_name ${source_file} NAME_WE)
                list(APPEND RUN_TESTS_COMMAND 
                    COMMAND $<TARGET_FILE:${test_name}> -r junit -o ${CATCH2_OUTPUT_DIR}/${test_name}.xml
                )
            endforeach()
            
            add_custom_target(coverage
                COMMAND ${LCOV_EXECUTABLE} --directory . --zerocounters
                ${RUN_TESTS_COMMAND}  # Run tests individually instead of ctest
                COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV_EXECUTABLE} --extract coverage.info 
                    '${CMAKE_SOURCE_DIR}/src/*'
                    '${CMAKE_SOURCE_DIR}/include/*'
                    --output-file coverage_filtered.info
                COMMAND ${GENHTML_EXECUTABLE} coverage_filtered.info --output-directory ${COVERAGE_OUTPUT_DIR}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating GCC code coverage report"
                DEPENDS all_unit_tests
            )
            
            message(STATUS "GCC coverage target created. Run 'cmake --build . --target coverage'")
        else()
            message(WARNING "lcov and/or genhtml not found. Coverage target not available.")
        endif()
        
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLVM_PROFDATA_EXECUTABLE llvm-profdata)
        find_program(LLVM_COV_EXECUTABLE llvm-cov)
        
        if(LLVM_PROFDATA_EXECUTABLE AND LLVM_COV_EXECUTABLE)
            # Build list of all test executables from UNIT_TESTS
            set(TEST_EXECUTABLES "")
            foreach(source_file ${UNIT_TESTS})
                get_filename_component(test_name ${source_file} NAME_WE)
                list(APPEND TEST_EXECUTABLES $<TARGET_FILE:${test_name}>)
            endforeach()
            
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage.profraw" ctest --output-on-failure
                COMMAND ${LLVM_PROFDATA_EXECUTABLE} merge -sparse ${CMAKE_BINARY_DIR}/coverage.profraw -o ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${LLVM_COV_EXECUTABLE} show ${TEST_EXECUTABLES} --format=html --output-dir=${COVERAGE_OUTPUT_DIR} --instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating Clang code coverage report"
                DEPENDS all_unit_tests
            )
            
            message(STATUS "Clang coverage target created. Run 'cmake --build . --target coverage'")
        else()
            message(WARNING "llvm-profdata and/or llvm-cov not found. Coverage target not available.")
        endif()
    endif()
endif()

############################
### Convenience Targets ###
############################

# Run all unit tests
add_custom_target(run_unit_tests
    COMMAND ctest --output-on-failure -L unit
    DEPENDS all_unit_tests
    COMMENT "Running all unit tests"
)

# Test with verbose output
add_custom_target(test_verbose
    COMMAND ctest --output-on-failure --verbose -L unit
    DEPENDS all_unit_tests
    COMMENT "Running tests with verbose output"
)

# Quick test run for development
add_custom_target(test_quick
    COMMAND ctest --output-on-failure -L unit --parallel
    DEPENDS all_unit_tests
    COMMENT "Running tests in parallel (quick)"
)
