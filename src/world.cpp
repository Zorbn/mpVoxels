#include "world.hpp"

#include "chunk.hpp"

World::World(int32_t chunkSize, int32_t mapSizeInChunks) : chunkSize(chunkSize), mapSizeInChunks(mapSizeInChunks) {
    mapSize = chunkSize * mapSizeInChunks;

    int32_t mapChunkCount = mapSizeInChunks * mapSizeInChunks * mapSizeInChunks;
    chunks.reserve(mapChunkCount);

    for (int32_t i = 0; i < mapChunkCount; i++) {
        int32_t x = i % mapSizeInChunks;
        int32_t y = (i / mapSizeInChunks) % mapSizeInChunks;
        int32_t z = i / (mapSizeInChunks * mapSizeInChunks);
        chunks.push_back(Chunk(chunkSize, x, y, z));
    }
}

Chunk& World::getChunk(int32_t x, int32_t y, int32_t z) {
    return chunks[x + y * mapSizeInChunks + z * mapSizeInChunks * mapSizeInChunks];
}

void World::updateChunk(int32_t x, int32_t y, int32_t z) {
    if (x < 0 || x >= mapSizeInChunks || y < 0 || y >= mapSizeInChunks || z < 0 || z >= mapSizeInChunks) return;

    getChunk(x, y, z).needsUpdate = true;
}

void World::setBlock(int32_t x, int32_t y, int32_t z, Blocks block) {
    if (x < 0 || x >= mapSize || y < 0 || y >= mapSize || z < 0 || z >= mapSize) return;

    int32_t chunkX = x / chunkSize;
    int32_t chunkY = y / chunkSize;
    int32_t chunkZ = z / chunkSize;
    int32_t localX = x % chunkSize;
    int32_t localY = y % chunkSize;
    int32_t localZ = z % chunkSize;
    Chunk& chunk = getChunk(chunkX, chunkY, chunkZ);
    if (!chunk.setBlock(localX, localY, localZ, block)) return;

    int32_t maxPos = chunkSize - 1;

    if (localX == 0)      updateChunk(chunkX - 1, chunkY, chunkZ);
    if (localX == maxPos) updateChunk(chunkX + 1, chunkY, chunkZ);
    if (localY == 0)      updateChunk(chunkX, chunkY - 1, chunkZ);
    if (localY == maxPos) updateChunk(chunkX, chunkY + 1, chunkZ);
    if (localZ == 0)      updateChunk(chunkX, chunkY, chunkZ - 1);
    if (localZ == maxPos) updateChunk(chunkX, chunkY, chunkZ + 1);
}

Blocks World::getBlock(int32_t x, int32_t y, int32_t z) {
    if (x < 0 || x >= mapSize || y < 0 || y >= mapSize || z < 0 || z >= mapSize) return Blocks::Dirt;

    int32_t chunkX = x / chunkSize;
    int32_t chunkY = y / chunkSize;
    int32_t chunkZ = z / chunkSize;
    int32_t localX = x % chunkSize;
    int32_t localY = y % chunkSize;
    int32_t localZ = z % chunkSize;
    Chunk& chunk = getChunk(chunkX, chunkY, chunkZ);
    return chunk.getBlock(localX, localY, localZ);
}

bool World::isBlockOccupied(int32_t x, int32_t y, int32_t z) {
    return getBlock(x, y, z) != Blocks::Air;
}

bool World::isBlockSupported(int32_t x, int32_t y, int32_t z) {
    return isBlockOccupied(x + 1, y, z) ||
        isBlockOccupied(x - 1, y, z) ||
        isBlockOccupied(x, y + 1, z) ||
        isBlockOccupied(x, y - 1, z) ||
        isBlockOccupied(x, y, z + 1) ||
        isBlockOccupied(x, y, z - 1);
}

std::optional<glm::vec3> World::getSpawnPos(int32_t spawnChunkX, int32_t spawnChunkY, int32_t spawnChunkZ, bool force) {
    int32_t spawnChunkWorldX = spawnChunkX * chunkSize;
    int32_t spawnChunkWorldY = spawnChunkY * chunkSize;
    int32_t spawnChunkWorldZ = spawnChunkZ * chunkSize;

    Chunk& spawnChunk = getChunk(spawnChunkX, spawnChunkY, spawnChunkZ);

    for (int32_t z = 0; z < chunkSize; z++) {
        for (int32_t y = 0; y < chunkSize; y++) {
            for (int32_t x = 0; x < chunkSize; x++) {
                if (spawnChunk.getBlock(x, y, z) != Blocks::Air) continue;

                return std::optional<glm::vec3>{{
                    spawnChunkWorldX + x + 0.5,
                    spawnChunkWorldY + y + 0.5,
                    spawnChunkWorldZ + z + 0.5,
                }};
            }
        }
    }

    if (force) {
        int32_t x = chunkSize / 2;
        int32_t y = chunkSize / 2;
        int32_t z = chunkSize / 2;
        spawnChunk.setBlock(x, y, z, Blocks::Air);

        return std::optional<glm::vec3>{{
            spawnChunkWorldX + x + 0.5,
            spawnChunkWorldY + y + 0.5,
            spawnChunkWorldZ + z + 0.5,
        }};
    }

    return std::nullopt;
}

void World::generate(std::mt19937& rng, siv::BasicPerlinNoise<float>& noise) {
    for (int32_t z = 0; z < mapSizeInChunks; z++) {
        for (int32_t y = 0; y < mapSizeInChunks; y++) {
            for (int32_t x = 0; x < mapSizeInChunks; x++) {
                getChunk(x, y, z).generate(rng, noise);
            }
        }
    }
}

void World::draw(VkCommandBuffer commandBuffer) {
    for (int32_t i = 0; i < chunks.size(); i++) {
        chunks[i].draw(commandBuffer);
    }
}

void World::destroy(VmaAllocator allocator) {
    for (int32_t i = 0; i < chunks.size(); i++) {
        chunks[i].destroy(allocator);
    }
}

void World::update(VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device) {
    for (int32_t i = 0; i < chunks.size(); i++) {
        chunks[i].update(*this, allocator, commands, graphicsQueue, device);
    }
}