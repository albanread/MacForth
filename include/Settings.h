#ifndef SETTINGS_H
#define SETTINGS_H

#include <deque>
#include "Tokenizer.h"
#include "CodeGenerator.h"

inline bool print_stack = false;
inline bool optimizer;
inline bool jitLogging = false;
inline bool debug = false;
inline bool GPCACHE = false;
inline bool TrackLRU = true;
inline int corePinned = 0;
inline bool corePinnedSet = false;


inline void display_settings() {
    std::cout << "Current Settings:" << std::endl;
    std::cout << "Stack prompt: " << (print_stack ? "ON" : "OFF") << std::endl;
    std::cout << "Optimizer: " << (optimizer ? "ON" : "OFF") << std::endl;
    std::cout << "JIT logging: " << (jitLogging ? "ON" : "OFF") << std::endl;
    std::cout << "Debug mode: " << (debug ? "ON" : "OFF") << std::endl;
    std::cout << "GPCACHE: " << (GPCACHE ? "ON" : "OFF") << std::endl;
    std::cout << "Track LRU: " << (TrackLRU ? "ON" : "OFF") << std::endl;
    std::cout << "Core pinned: " << (corePinnedSet ? "ON" : "OFF") << std::endl;
    if (corePinnedSet) {
        std::cout << "Core pinned to: " << (corePinned == 0 ? "Core 0" : (corePinned == 1 ? "Core 1" : (corePinned == 2 ? "Core 2" : (corePinned == 3 ? "Core 3" : "Core 4")))) << std::endl;
    }
    std::cout << std::endl;
}

inline void display_set_help() {
std::cout << "Usage: SET <feature> <state>" << std::endl;
    std::cout << "Available features:" << std::endl;
    std::cout << "  STACKPROMPT ON/OFF" << std::endl;
    std::cout << "  DEBUG ON/OFF" << std::endl;
    std::cout << "  GPCACHE ON/OFF" << std::endl;
    std::cout << "  LOGGING ON/OFF" << std::endl;
    std::cout << "  OPTIMIZE ON/OFF" << std::endl;
    std::cout << "  TRACKLRU ON/OFF" << std::endl;
    std::cout << "  CORE ZERO,ONE,TWO,THREE,FOUR|ANY" << std::endl;
    std::cout << std::endl;
    display_settings();
}



// SET THING ON,OFF
inline void runImmediateSET(std::deque<ForthToken> &tokens) {
    // we arrived here from TOKEN SET

    const ForthToken second = tokens.front();
    tokens.erase(tokens.begin()); // Remove the processed token
    if (tokens.empty()) {
        display_set_help();
        return; // Exit early if no tokens to process
    }

    auto feature = second.value;

    const ForthToken third = tokens.front();
    tokens.erase(tokens.begin()); // Remove the processed token
    if (tokens.empty()) return; // Exit early if no tokens to process
    auto state = third.value;

    if (feature == "CORE") {
        if (state == "ZERO") {
            pinToCore(0);
            corePinned = 0;
            corePinnedSet = true;
        } else if (state == "ONE") {
            pinToCore(1);
            corePinned = 1;
            corePinnedSet = true;
        } else if (state == "TWO") {
            pinToCore(2);
            corePinned = 2;
            corePinnedSet = true;
        } else if (state == "THREE") {
            pinToCore(3);
            corePinned = 3;
            corePinnedSet = true;
        } else if (state == "FOUR") {
            pinToCore(4);
            corePinned = 4;
            corePinnedSet = true;
        } else if (state == "ANY") {
            unpinThread();
            corePinned = 0;
            corePinnedSet = false;
            print_stack = false;
            std::cout << "Thread unpinned" << std::endl;
        }
    }


    if (feature == "STACKPROMPT") {
        if (state == "ON") {
            print_stack = true;
            std::cout << "Stack prompt on" << std::endl;
        } else if (state == "OFF") {
            print_stack = false;
            std::cout << "Stack prompt off" << std::endl;
        }
    }

    if ( feature == "DEBUG" ) {
        if (state == "ON") {
            debug = true;
            std::cout << "Debug mode on" << std::endl;
        } else if (state == "OFF") {
            debug = false;
            std::cout << "Debug mode off" << std::endl;
        }
    }

    if (feature == "GPCACHE") {
        if (state == "ON") {
            GPCACHE = true;
            std::cout << "GPCACHE enabled" << std::endl;
        } else if (state == "OFF") {
            GPCACHE = false;
            std::cout << "GPCACHE disabled" << std::endl;
        }
    }

    if (feature == "LOGGING") {
        if (state == "ON") {
            jitLogging = true;
            std::cout << "Logging enabled" << std::endl;
        } else if (state == "OFF") {
            jitLogging = false;
            std::cout << "Logging disabled" << std::endl;
        }
    }

    if (feature == "TRACKLRU") {
        if (state == "ON") {
            TrackLRU = true;
            std::cout << "LRU tracking enabled" << std::endl;
        } else if (state == "OFF") {
            TrackLRU = false;
            std::cout << "LRU tracking disabled" << std::endl;
        }
    }

    if (feature == "OPTIMIZE") {
        if (state == "ON") {
            optimizer = true;
            std::cout << "Optimizer enabled" << std::endl;
        } else if (state == "OFF") {
            optimizer = false;
            std::cout << "Optimizer disabled" << std::endl;
        }
    }
}



#endif //SETTINGS_H
