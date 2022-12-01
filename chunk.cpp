#include "chunk.hpp"

#include "world.hpp"

const float shadeNoiseScale = 0.2f;
const float caveNoiseScale = 0.1f;
const float caveNoiseSolidThreshold = 0.4f;

Chunk::Chunk(int32_t size, int32_t x, int32_t y, int32_t z) {
    this->chunkX = x;
    this->chunkY = y;
    this->chunkZ = z;
    this->size = size;

    data.resize(size * size * size);
    shadeNoise.resize(size * size * size);
    instances.push_back(InstanceData{glm::vec3(chunkX * size, chunkY * size, chunkZ * size)});
}

int32_t Chunk::getBlockIndex(int32_t x, int32_t y, int32_t z) {
    return x + y * size + z * size * size;
}

bool Chunk::setBlock(int32_t x, int32_t y, int32_t z, Blocks type) {
    if (x < 0 || x >= size || y < 0 || y >= size || z < 0 || z >= size) return false;

    data[getBlockIndex(x, y, z)] = type;
    needsUpdate = true;

    return true;
}

Blocks Chunk::getBlock(int32_t x, int32_t y, int32_t z) {
    if (x < 0 || x >= size || y < 0 || y >= size || z < 0 || z >= size) return Blocks::Air;

    return data[getBlockIndex(x, y, z)];
}

void Chunk::update(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device) {
    if (needsUpdate) {
        updateMesh(world, allocator, commands, graphicsQueue, device);
        needsUpdate = false;
    }
}

void Chunk::updateMesh(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device) {
    vertices.clear();
    indices.clear();

    for (int32_t z = 0; z < size; z++) {
        for (int32_t y = 0; y < size; y++) {
            for (int32_t x = 0; x < size; x++) {
                int32_t worldX = x + chunkX * size;
                int32_t worldY = y + chunkY * size;
                int32_t worldZ = z + chunkZ * size;
                Blocks block = world.getBlock(worldX, worldY, worldZ);
                float noiseValue = shadeNoise[x + y * size + z * size * size];

                if (block == Blocks::Air) continue;

                for (int32_t face = 0; face < 6; face++) {
                    if (world.getBlock(worldX + directions[face][0], worldY + directions[face][1],
                                worldZ + directions[face][2]) != Blocks::Air)
                        continue;

                    size_t vertexCount = vertices.size();
                    for (uint32_t index : cubeIndices[face]) {
                        indices.push_back(index + static_cast<uint32_t>(vertexCount));
                    }

                    for (size_t i = 0; i < 4; i++) {
                        glm::vec3 vertex = cubeVertices[face][i];
                        glm::vec2 uv = cubeUvs[face][i];

                        vertices.push_back(VertexData {
                            vertex + glm::vec3(x, y, z),
                            glm::vec3(1.0, 1.0, 1.0),
                            glm::vec3(uv.x, uv.y, static_cast<float>(block) - 1),
                        });
                    }
                }
            }
        }
    }

    if (firstUpdate) {
        model = Model<VertexData, uint32_t, InstanceData>::fromVerticesAndIndices(vertices, indices, 1, allocator, commands, graphicsQueue, device);
        model.updateInstances(instances, commands, allocator, graphicsQueue, device);
    } else {
        model.update(vertices, indices, commands, allocator, graphicsQueue, device);
    }
}

void Chunk::generate(std::mt19937& rng, siv::BasicPerlinNoise<float>& noise) {
    for (int32_t z = 0; z < size; z++) {
        for (int32_t y = 0; y < size; y++) {
            for (int32_t x = 0; x < size; x++) {
                int32_t worldX = x + chunkX * size;
                int32_t worldY = y + chunkY * size;
                int32_t worldZ = z + chunkZ * size;

                float shadeNoiseValue = noise.noise3D(worldX * shadeNoiseScale, worldY * shadeNoiseScale, worldZ * shadeNoiseScale);
                shadeNoise[x + y * size + z * size * size] = shadeNoiseValue;

                if (shouldGenerateSolid(noise, worldX, worldY, worldZ)) {
                    setBlock(x, y, z, Blocks::Dirt);
                }
            }
        }
    }
}

void Chunk::draw(VkCommandBuffer commandBuffer) {
    model.draw(commandBuffer);
}

void Chunk::destroy(VmaAllocator allocator) {
    model.destroy(allocator);
}

bool Chunk::shouldGenerateSolid(siv::BasicPerlinNoise<float>& noise, int32_t worldX, int32_t worldY, int32_t worldZ) {
    float noiseValue = noise.noise3D(worldX * caveNoiseScale, worldY * caveNoiseScale, worldZ * caveNoiseScale);
    return noiseValue < caveNoiseSolidThreshold;
}