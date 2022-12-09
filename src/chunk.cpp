#include "chunk.hpp"

#include "world.hpp"

const float shadeNoiseScale = 0.2f;
const float hillNoiseScale = 0.05f;
const float hillHeight = 64.0f;
const float valleyHeight = 32.0f;
const float caveNoiseScale = 0.1f;
const float caveNoiseSolidThreshold = 0.7f;

Chunk::Chunk(int32_t size, int32_t x, int32_t y, int32_t z) : chunkX(x), chunkY(y), chunkZ(z), size(size) {
    data.resize(size * size * size);
    shadeNoise.resize(size * size * size);
    lightMap.resize(size * size * size, true);
    instances.push_back(InstanceData{glm::vec3(chunkX * size, chunkY * size, chunkZ * size)});
}

int32_t Chunk::getBlockIndex(int32_t x, int32_t y, int32_t z) {
    return x + y * size + z * size * size;
}

bool Chunk::setBlock(World& world, int32_t x, int32_t y, int32_t z, Blocks type) {
    if (x < 0 || x >= size || y < 0 || y >= size || z < 0 || z >= size) return false;

    data[getBlockIndex(x, y, z)] = type;
    needsUpdate = true;

    setLitColumn(world, x, y - 1, z, type == Blocks::Air ? getLit(x, y, z) : false);

    return true;
}

Blocks Chunk::getBlock(int32_t x, int32_t y, int32_t z) {
    if (x < 0 || x >= size || y < 0 || y >= size || z < 0 || z >= size) return Blocks::Air;

    return data[getBlockIndex(x, y, z)];
}

bool Chunk::isBlockOccupied(int32_t x, int32_t y, int32_t z) {
    return getBlock(x, y, z) != Blocks::Air;
}

void Chunk::setLitColumn(World& world, int32_t x, int32_t topY, int32_t z, bool lit) {
    for (int32_t y = topY; y >= 0; y--) {
        if (getLit(x, y, z) == lit) {
            return;
        }

        setLit(x, y, z, lit);

        if (isBlockOccupied(x, y, z)) {
            return;
        }
    }

    if (chunkY == 0) {
        return;
    }

    world.getChunk(chunkX, chunkY - 1, chunkZ).setLitColumn(world, x, size - 1, z, lit);
}

void Chunk::setLit(int32_t x, int32_t y, int32_t z, bool lit) {
    lightMap[y + x * size + z * size * size] = lit;
    needsUpdate = true;
}

bool Chunk::getLit(int32_t x, int32_t y, int32_t z) {
    return lightMap[y + x * size + z * size * size];
}

bool Chunk::update(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device) {
    if (needsUpdate) {
        updateMesh(world, allocator, commands, graphicsQueue, device);
        needsUpdate = false;
        return true;
    }

    return false;
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

                if (block == Blocks::Air) continue;

                float noiseValue = shadeNoise[x + y * size + z * size * size];

                for (int32_t face = 0; face < 6; face++) {
                    if (world.isBlockOccupied(worldX + directions[face][0], worldY + directions[face][1],
                                worldZ + directions[face][2]))
                        continue;

                    float lightLevel = world.getLit(worldX + directions[face][0],
                                                    worldY + directions[face][1],
                                                    worldZ + directions[face][2]) ? 1.0f : 0.5f;

                    size_t vertexCount = vertices.size();
                    for (uint32_t index : cubeIndices[face]) {
                        indices.push_back(index + static_cast<uint32_t>(vertexCount));
                    }

                    for (size_t i = 0; i < 4; i++) {
                        glm::vec3 vertex = cubeVertices[face][i];
                        glm::vec2 uv = cubeUvs[face][i];

                        vertices.push_back(VertexData {
                            vertex + glm::vec3(x, y, z),
                            (cubeFaceColors[face] * 0.6f + noiseValue * 0.4f) * lightLevel,
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
        firstUpdate = false;
    } else {
        model.update(vertices, indices, commands, allocator, graphicsQueue, device);
    }
}

void Chunk::generate(World& world, std::mt19937& rng, siv::BasicPerlinNoise<float>& noise) {
    for (int32_t z = 0; z < size; z++) {
        int32_t worldZ = z + chunkZ * size;

        for (int32_t x = 0; x < size; x++) {
            int32_t worldX = x + chunkX * size;

            int32_t maxHeight = floorToInt(noise.noise2D_01(worldX * hillNoiseScale, worldZ * hillNoiseScale) * hillHeight + valleyHeight);

            for (int32_t y = 0; y < size; y++) {
                int32_t worldY = y + chunkY * size;

                float shadeNoiseValue = noise.noise3D_01(worldX * shadeNoiseScale, worldY * shadeNoiseScale, worldZ * shadeNoiseScale);
                shadeNoise[x + y * size + z * size * size] = shadeNoiseValue;

                if (worldY > maxHeight) continue;

                if (shouldGenerateSolid(noise, worldX, worldY, worldZ)) {
                    setBlock(world, x, y, z, Blocks::Dirt);
                }
            }
        }
    }
}

void Chunk::draw(VkCommandBuffer commandBuffer) {
    model.draw(commandBuffer);
}

glm::vec3 Chunk::getPos() {
    return glm::vec3(chunkX * size, chunkY * size, chunkZ * size);
}

glm::vec3 Chunk::getSize() {
    return glm::vec3(size, size, size);
}

void Chunk::destroy(VmaAllocator allocator) {
    model.destroy(allocator);
}

bool Chunk::shouldGenerateSolid(siv::BasicPerlinNoise<float>& noise, int32_t worldX, int32_t worldY, int32_t worldZ) {
    float noiseValue = noise.noise3D_01(worldX * caveNoiseScale, worldY * caveNoiseScale, worldZ * caveNoiseScale);
    return noiseValue < caveNoiseSolidThreshold;
}