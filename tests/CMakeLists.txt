# Unit tests. Run them with ctest from the build directory.
#
# The tests link CADxCore; only the objects they use are pulled in, so they
# need the libraries those objects depend on, not the whole application.

find_package(GTest REQUIRED)
include(GoogleTest)

ADD_GLOBAL_CORE_DEFINITIONS()

INCLUDE_DIRECTORIES(${CADxCore_INCLUDE_DIR})
FIND_PACKAGE(wxWidgets 3.0.1 COMPONENTS core base html gl net xml aui adv richtext xrc propgrid REQUIRED)
foreach(d ${wxWidgets_DEFINITIONS})
  ADD_DEFINITIONS("-D${d}")
endforeach()
INCLUDE_DIRECTORIES(${wxWidgets_INCLUDE_DIRS})

add_executable(ginkgo_tests
  phantom.cpp
  stubs.cpp
  test_geometry.cpp
  test_gldraw.cpp
  test_seriesgeometry.cpp
  test_seriesvolume.cpp
  test_tlslayer.cpp
)

target_link_libraries(ginkgo_tests
  CADxCore
  GTest::gtest_main
  ${VTK_LIBRARIES}
  ${DCMTK_LIBRARIES}
  ${wxWidgets_LIBRARIES}
  ssl
  crypto
)
vtk_module_autoinit(TARGETS ginkgo_tests MODULES ${VTK_LIBRARIES})

gtest_discover_tests(ginkgo_tests)

# Writes the phantom CT series for checking the 3D views by eye (not run by ctest)
add_executable(make_phantom tools/make_phantom.cpp phantom.cpp)
target_include_directories(make_phantom PRIVATE ${DCMTK_INCLUDE_DIRS})
target_link_libraries(make_phantom ${DCMTK_LIBRARIES} oflog)
