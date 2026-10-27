# Unit tests. Run them with ctest from the build directory.
#
# The tests link CADxCore; only the objects they use are pulled in, so they
# need the libraries those objects depend on, not the whole application.

find_package(GTest REQUIRED)
include(GoogleTest)

ADD_GLOBAL_CORE_DEFINITIONS()

INCLUDE_DIRECTORIES(${CADxCore_INCLUDE_DIR})
FIND_PACKAGE(wxWidgets 3.0.1 REQUIRED)
foreach(d ${wxWidgets_DEFINITIONS})
  ADD_DEFINITIONS("-D${d}")
endforeach()
INCLUDE_DIRECTORIES(${wxWidgets_INCLUDE_DIRS})

add_executable(ginkgo_tests
  test_geometry.cpp
  test_gldraw.cpp
  test_seriesgeometry.cpp
  test_tlslayer.cpp
)

target_link_libraries(ginkgo_tests
  CADxCore
  GTest::gtest_main
  ${VTK_LIBRARIES}
  ${DCMTK_LIBRARIES}
  ssl
  crypto
)
vtk_module_autoinit(TARGETS ginkgo_tests MODULES ${VTK_LIBRARIES})

gtest_discover_tests(ginkgo_tests)
