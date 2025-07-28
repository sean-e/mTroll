cmake_minimum_required(VERSION 3.16)

project(mTroll VERSION 2025.05.25 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

# Set Qt5 directories explicitly
set(Qt5_DIR "/opt/homebrew/opt/qt@5/lib/cmake/Qt5")
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt@5/lib/cmake" ${CMAKE_PREFIX_PATH})

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    find_library(COREMIDI_FRAMEWORK CoreMIDI)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
endif()

# Define preprocessor macros
add_compile_definitions(
    TIXML_USE_STL
    QT_DLL
    QT_GUI_LIB
    QT_CORE_LIB
    QT_THREAD_SUPPORT
    STRICT
    _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING
)

if(WIN32)
    add_compile_definitions(
        UNICODE
        WIN32
        _WINDOWS
    )
else()
    # Define _ASSERTE for non-Windows platforms
    add_compile_definitions(_ASSERTE=assert)
    # Include assert.h and cross-platform compatibility through compile definitions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include cassert -include ${CMAKE_CURRENT_SOURCE_DIR}/CrossPlatform.h")
    
    # Suppress deprecation warnings (like strstream)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
    
    # Define Windows-specific functions for cross-platform compatibility
    add_compile_definitions(
        _itoa_s=mtroll_itoa_s
        strcat_s=mtroll_strcat_s
    )
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/mTrollQt
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml
    ${CMAKE_CURRENT_SOURCE_DIR}/Monome40h
    ${CMAKE_CURRENT_SOURCE_DIR}/midi
    ${CMAKE_CURRENT_SOURCE_DIR}/winUtil
)

# Engine source files
set(ENGINE_SOURCES
    Engine/AxeFx3Manager.cpp
    Engine/AxeFx3Manager.h
    Engine/AxeFxManager.cpp
    Engine/AxeFxManager.h
    Engine/AxemlLoader.cpp
    Engine/AxemlLoader.h
    Engine/ControllerInputMonitor.cpp
    Engine/ControllerInputMonitor.h
    Engine/DynamicMidiCommand.cpp
    Engine/DynamicMidiCommand.h
    Engine/EdpManager.cpp
    Engine/EdpManager.h
    Engine/EngineLoader.cpp
    Engine/EngineLoader.h
    Engine/ExpressionPedals.cpp
    Engine/ExpressionPedals.h
    Engine/HexStringUtils.cpp
    Engine/HexStringUtils.h
    Engine/MidiControlEngine.cpp
    Engine/MidiControlEngine.h
    Engine/Patch.cpp
    Engine/Patch.h
    Engine/PatchBank.cpp
    Engine/PatchBank.h
    Engine/PersistentPedalOverridePatch.cpp
    Engine/PersistentPedalOverridePatch.h
    Engine/TwoStatePatch.cpp
    Engine/TwoStatePatch.h
    Engine/UiLoader.cpp
    Engine/UiLoader.h
)

# TinyXML source files
set(TINYXML_SOURCES
    tinyxml/tinyxml.cpp
    tinyxml/tinyxml.h
    tinyxml/tinyxmlerror.cpp
    tinyxml/tinyxmlparser.cpp
    tinyxml/tinystr.cpp
    tinyxml/tinystr.h
)

# Platform-specific MIDI sources
if(PLATFORM_WINDOWS)
    set(MIDI_SOURCES
        midi/WinMidiIn.cpp
        midi/WinMidiIn.h
        midi/WinMidiOut.cpp
        midi/WinMidiOut.h
        midi/SleepShort.cpp
        midi/SleepShort.h
    )
    set(PLATFORM_LIBS winmm)
elseif(PLATFORM_MACOS)
    set(MIDI_SOURCES
        midi/MacMidiIn.cpp
        midi/MacMidiIn.h
        midi/MacMidiOut.cpp
        midi/MacMidiOut.h
        midi/SleepShort.cpp
        midi/SleepShort.h
    )
    set(PLATFORM_LIBS ${COREMIDI_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
else()
    # Linux/other platforms would need ALSA MIDI implementation
    message(WARNING "MIDI implementation not available for this platform")
    set(MIDI_SOURCES)
    set(PLATFORM_LIBS)
endif()

# Monome source files (Windows only due to FTDI dependency)
if(PLATFORM_WINDOWS)
    set(MONOME_SOURCES
        Monome40h/qt/Monome40hFtqt.cpp
        Monome40h/qt/Monome40hFtqt.h
    )
else()
    set(MONOME_SOURCES)
endif()

# Platform-specific utility sources
if(PLATFORM_WINDOWS)
    set(UTIL_SOURCES
        winUtil/WinDark.cpp
        winUtil/WinDark.h
    )
else()
    set(UTIL_SOURCES)
endif()

# Qt source files
set(QT_SOURCES
    mTrollQt/AboutDlg.cpp
    mTrollQt/AboutDlg.h
    mTrollQt/BuildDate.cpp
    mTrollQt/ControlUi.cpp
    mTrollQt/ControlUi.h
    mTrollQt/main.cpp
    mTrollQt/MainTrollWindow.cpp
    mTrollQt/MainTrollWindow.h
)

# Qt MOC files
qt5_wrap_cpp(QT_MOC_SOURCES
    Engine/AxeFx3Manager.h
    Engine/AxeFxManager.h
    mTrollQt/ControlUi.h
    mTrollQt/MainTrollWindow.h
)

# Windows resource file
if(PLATFORM_WINDOWS)
    set(RESOURCE_FILES mTrollQt/win/mTrollQt.rc)
else()
    set(RESOURCE_FILES)
endif()

# Create the executable
add_executable(mTroll
    ${ENGINE_SOURCES}
    ${TINYXML_SOURCES}
    ${MIDI_SOURCES}
    ${MONOME_SOURCES}
    ${UTIL_SOURCES}
    ${QT_SOURCES}
    ${QT_MOC_SOURCES}
    ${RESOURCE_FILES}
)

# Link libraries
target_link_libraries(mTroll
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    ${PLATFORM_LIBS}
)

# Platform-specific linking
if(PLATFORM_WINDOWS)
    # Add FTDI library for Windows
    target_link_directories(mTroll PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Monome40h)
    target_link_libraries(mTroll FTD2XX)
    
    # Set subsystem to Windows
    set_target_properties(mTroll PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Set output directory
set_target_properties(mTroll PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS mTroll
    RUNTIME DESTINATION bin
)

# Install data files
install(DIRECTORY data/
    DESTINATION share/mTroll/data
    FILES_MATCHING PATTERN "*.xml" PATTERN "*.xsl"
)