#pragma once
#include <cstdint>

void OnBlockPlaced(int x, int y, int z, uint8_t blockType);
void OnBlockRemoved(int x, int y, int z);
void initLightingThread();
void stopLightingThread();
