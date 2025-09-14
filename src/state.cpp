#include "state.hpp"
#include "globals.hpp"

bool init_state() {
    State s = State{};
    s.mode = ids::MODE_PLAYING;
    ss = &s;
    
    return true;
}
