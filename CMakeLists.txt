include(CPack)
include(FetchContent)

cmake_minimum_required(VERSION 3.0.0)

project(mpVoxels VERSION 0.1.0)
set(PROJ_NAME mpVoxels)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
)
FetchContent_Declare(
        vk_mem_alloc
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.0.1
)
FetchContent_Declare(
    vkFrame
    GIT_REPOSITORY https://github.com/Zorbn/vkFrame.git
    GIT_TAG 63fb90b368f345a7127cebaf64de28171756343c
)

FetchContent_MakeAvailable(vkFrame glfw glm vk_mem_alloc)

add_executable(
    ${PROJ_NAME}
    src/main.cpp
    src/renderTypes.hpp
    src/cubeMesh.hpp
    src/primitiveMeshes.hpp
    src/blocks.hpp
    src/directions.hpp
    src/blockInteraction.cpp src/blockInteraction.hpp
    src/physics.cpp src/physics.hpp
    src/gameMath.cpp src/gameMath.hpp
    src/world.cpp src/world.hpp
    src/chunk.cpp src/chunk.hpp
    src/player.cpp src/player.hpp
    src/implementations.cpp
    deps/perlinNoise.hpp
    deps/enet.h
)

target_link_libraries(
    ${PROJ_NAME}
    vkFrame
    glm
    glfw
    Vulkan::Vulkan
    VulkanMemoryAllocator
)