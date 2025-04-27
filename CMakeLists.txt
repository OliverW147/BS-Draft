cmake_minimum_required(VERSION 3.16)
project(GlizzyDraft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# Define source files
# Define source files
set(PROJECT_SOURCES
    main.cpp
    MainWindow.h MainWindow.cpp
    AppConfig.h AppConfig.cpp
    DataStructures.h DataStructures.cpp   # <-- ADD DataStructures.cpp HERE
    DataLoader.h DataLoader.cpp
    StatsCalculator.h StatsCalculator.cpp
    DraftState.h DraftState.cpp
    Heuristics.h Heuristics.cpp
    MCTS.h MCTS.cpp
    CacheUtils.h CacheUtils.cpp
    resources.qrc
)

# Create the executable
qt_add_executable(GlizzyDraft ${PROJECT_SOURCES})

# Link Qt libraries
target_link_libraries(GlizzyDraft PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

# Installation (optional, but good practice)
install(TARGETS GlizzyDraft
    RUNTIME DESTINATION bin # Installs executable to 'bin' subdir of install prefix
)