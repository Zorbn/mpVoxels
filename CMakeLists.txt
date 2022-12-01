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
    GIT_TAG 5c68d1807ecd01e718943e953a28aae4d4ff3de8
)

FetchContent_MakeAvailable(vkFrame glfw glm vk_mem_alloc)

add_executable(
    ${PROJ_NAME}
    main.cpp
    renderTypes.hpp
    cubeMesh.hpp
    blocks.hpp
    physics.cpp physics.hpp
    gameMath.cpp gameMath.hpp
    world.cpp world.hpp
    chunk.cpp chunk.hpp
    directions.hpp
    deps/perlinNoise.hpp
)

target_link_libraries(
    ${PROJ_NAME}
    vkFrame
    glm
    glfw
    Vulkan::Vulkan
    VulkanMemoryAllocator
)