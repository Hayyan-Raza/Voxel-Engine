#pragma once
#include "ChunkManager.h"

struct SaveTask {
    int cx, cz;
    ChunkData* chunks[WORLD_HEIGHT_CHUNKS]; // Array of pointers to detached chunk data
};

void generateTerrain(unsigned int seed);
bool isTerrainGenerating();

void queueChunkSave(const SaveTask& task);
void initSaveThread();
void stopSaveThread();

void queueChunkGeneration(int cx, int cz);
void initGenerationThreads();
void stopGenerationThreads();
