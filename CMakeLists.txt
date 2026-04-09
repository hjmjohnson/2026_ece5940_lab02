cmake_minimum_required(VERSION 3.16)
project(RegisterCircles LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── ITK ──────────────────────────────────────────────────────────────────────
find_package(ITK 6 REQUIRED
  COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIOMHA
    ITKRegistrationMethodsv4
    ITKMetricsv4
    ITKOptimizersv4
    ITKTransform
    ITKImageFunction
    ITKSpatialObjects
    ITKImageSources
)
include(${ITK_USE_FILE})

# Enforce ITK ≥ 6 at configure time
if(ITK_VERSION VERSION_LESS "6.0")
  message(FATAL_ERROR
    "This project requires ITK >= 6.0 (found ${ITK_VERSION}).")
endif()

# ── Targets ───────────────────────────────────────────────────────────────────
add_executable(GenerateCircleImages src/GenerateCircleImages.cxx)
target_link_libraries(GenerateCircleImages PRIVATE ${ITK_LIBRARIES})

add_executable(RegisterCircles src/RegisterCircles.cxx)
target_link_libraries(RegisterCircles PRIVATE ${ITK_LIBRARIES})

# ── CTest integration (optional) ──────────────────────────────────────────────
enable_testing()

add_test(NAME GenerateImages
  COMMAND GenerateCircleImages
    "${CMAKE_BINARY_DIR}/circle_fixed.mha"
    "${CMAKE_BINARY_DIR}/circle_moving.mha"
)

add_test(NAME RegisterImages
  COMMAND RegisterCircles
    "${CMAKE_BINARY_DIR}/circle_fixed.mha"
    "${CMAKE_BINARY_DIR}/circle_moving.mha"
    "${CMAKE_BINARY_DIR}/registered_output.mha"
)
set_tests_properties(RegisterImages PROPERTIES DEPENDS GenerateImages)
