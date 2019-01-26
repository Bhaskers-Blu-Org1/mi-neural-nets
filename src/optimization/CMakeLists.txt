# Include current dir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

install(FILES
	OptimizationFunction.hpp
	OptimizationArray.hpp
	AdaDelta.hpp
	AdaGrad.hpp
	Adam.hpp
	GradientDescent.hpp
	Momentum.hpp
	RMSProp.hpp
	OptimizationFunctionTypes.hpp
	HebbianRule.hpp
	NormalizedHebbianRule.hpp
	NormalizedZerosumHebbianRule.hpp
	BinaryCorrelatorLearningRule.hpp
	DESTINATION include/optimization)


# =======================================================================
# Build artificial landscapes and optimization functions tests
# =======================================================================

# Link tests with GTest
if(GTEST_FOUND AND BUILD_UNIT_TESTS)

	add_executable(artificialLandscapesTestsRunner ArtificialLandscapesTests.cpp)
	target_link_libraries(artificialLandscapesTestsRunner ${GTEST_LIBRARIES})
	add_test(artificialLandscapesTestsRunner ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/artificialLandscapesTestsRunner)

	add_executable(optimizationFunctionsTestsRunner
		OptimizationFunctionsTests.cpp
		GradientDescentTests.cpp
		MomentumTests.cpp
		AdaGradTests.cpp
		RMSPropTests.cpp
		AdaDeltaTests.cpp
		AdamTests.cpp
		GradPIDTests.cpp
		AdamIDTests.cpp
		)
	target_link_libraries(optimizationFunctionsTestsRunner ${GTEST_LIBRARIES})
	add_test(optimizationFunctionsTestsRunner ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/optimizationFunctionsTestsRunner)

endif(GTEST_FOUND AND BUILD_UNIT_TESTS)


# =======================================================================
# Build hebbian learning test
# =======================================================================

# Link tests with GTest
if(GTEST_FOUND AND BUILD_UNIT_TESTS)

	add_executable(hebbianLearningTestsRunner
		HebbianRuleTests.cpp
		)
	target_link_libraries(hebbianLearningTestsRunner ${GTEST_LIBRARIES})
	add_test(optimizationFunctionsTestsRunner ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hebbianLearningTestsRunner)

endif(GTEST_FOUND AND BUILD_UNIT_TESTS)