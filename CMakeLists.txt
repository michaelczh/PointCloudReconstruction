
if (WIN32)
    cmake_minimum_required( VERSION 2.8 )

    # Create Project
    project( Reconstruction )
    add_executable( project extract_walls.cpp )

    # Set StartUp Project (Option)
    # (This setting is able to enable by using CMake 3.6.0 RC1 or later.)
    set_property( DIRECTORY PROPERTY VS_STARTUP_PROJECT "project" )

    # Find Packages
    find_package( PCL 1.8 REQUIRED )

    if( PCL_FOUND )
      # [C/C++]>[General]>[Additional Include Directories]
      include_directories( ${PCL_INCLUDE_DIRS} 
        ${PROJECT_SOURCE_DIR}/include)

      # [C/C++]>[Preprocessor]>[Preprocessor Definitions]
      add_definitions( ${PCL_DEFINITIONS} )
      
      # For Use Not PreCompiled Features 
      add_definitions( -DPCL_NO_PRECOMPILE )

      # [Linker]>[General]>[Additional Library Directories]
      link_directories( ${PCL_LIBRARY_DIRS} )

      # [Linker]>[Input]>[Additional Dependencies]
      target_link_libraries( project ${PCL_LIBRARIES} )
    endif()
endif (WIN32)

if(UNIX AND NOT APPLE)
    cmake_minimum_required( VERSION 2.8 )
    project( Reconstruction )

    set( CMAKE_BUILD_TYPE DEBUG )
    set( CMAKE_CXX_FLAGS "-std=c++11 -O3" )

    include_directories(include/)
    include_directories(src/)

    add_library(${PROJECT_NAME} SHARED
            src/Plane.cpp
            src/SimpleView.cpp
            src/Reconstruction.cpp
            src/SimpleView.cpp
            src/DxfExporter.cpp)

    # eigen
    include_directories( "/usr/include/eigen3/" )


    # pcl
    find_package( PCL 1.8 REQUIRED )
    include_directories( ${PCL_INCLUDE_DIRS} )

    link_directories(${PCL_LIBRARY_DIRS})
    add_definitions( ${PCL_DEFINITIONS} )

    target_link_libraries(${PROJECT_NAME}
            ${PCL_LIBRARIES})

    ## Google Test
    # Locate GTest
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})

    find_package(  yaml-cpp REQUIRED )
    target_link_libraries(${PROJECT_NAME} ${YAML_CPP_LIBRARIES})

    # Add test cpp file
    add_executable(UnitTest UnitTest/test.cpp)

    # Link test executable against gtest & gtest_main
    target_link_libraries(UnitTest ${PROJECT_NAME} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  pthread)


    add_executable(extractWall extract_walls.cpp)
    target_link_libraries (extractWall ${PROJECT_NAME})
    add_executable(DownSampling src/Downsampling.cpp)
    target_link_libraries (DownSampling ${PROJECT_NAME})

endif()