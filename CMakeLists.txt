cmake_minimum_required(VERSION 3.12)

project(SerialPortLSL
    DESCRIPTION "Read byte stream from Serial port and emit as an LSL stream"
    HOMEPAGE_URL "https://github.com/roberttoth02/App-SerialPortMac/"
    LANGUAGES CXX
    VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# Dependencies

## LSL
find_package(LSL REQUIRED
    HINTS ${LSL_INSTALL_ROOT}
    "${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/build/"
    "${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/build/install"
    "${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/out/build/x64-Release"
    "${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/out/install/x64-Release"
    "${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/cmake-build-release/install"
    PATH_SUFFIXES share/LSL
)

## Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

## Boost
find_package(Boost REQUIRED COMPONENTS thread)

## Threads
find_package(Threads REQUIRED)

## Mac
set(MACOSX_BUNDLE TRUE) 
set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist)
set(MACOSX_BUNDLE_DIR "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app")
set(ICON_DESTINATION "${MACOSX_BUNDLE_DIR}/Contents/Resources/")
add_executable(${PROJECT_NAME} MACOSX_BUNDLE)

# Targets

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Boost::headers
    Boost::thread
    Threads::Threads
    LSL::lsl
)

installLSLApp(${PROJECT_NAME})
installLSLAuxFiles(${PROJECT_NAME}
    ${PROJECT_NAME}.cfg
    LICENSE
    README.md
)

get_target_property(QT_QMAKE_EXECUTABLE Qt::qmake IMPORTED_LOCATION)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:LSL::lsl>
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}//${PROJECT_NAME}.cfg
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${ICON_DESTINATION}
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}//icon.icns
    ${ICON_DESTINATION})

if(Qt6_FOUND)
    qt_finalize_executable(${PROJECT_NAME})
endif(Qt6_FOUND)

# Packaging Info
LSLGenerateCPackConfig()
