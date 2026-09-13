// =============================================================================
// TearDown Clone Engine — main.cpp  (orchestrator only)
// All systems live in src/core/, physics/, rendering/, particles/, input/
// =============================================================================

#include <iostream>
#include "core/Engine.h"

int main(int argc, char** argv) {
    Engine engine;
    
    if (!engine.init()) {
        std::cerr << "Failed to initialize engine.\n";
        return -1;
    }
    
    engine.run();
    engine.shutdown();
    
    return 0;
}
