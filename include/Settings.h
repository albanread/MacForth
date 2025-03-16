#ifndef SETTINGS_H
#define SETTINGS_H

#include <deque>
#include "Tokenizer.h"

inline bool print_stack = false;
inline bool optimizer;
inline bool jitLogging = false;
inline bool debug = false;
inline bool GPCACHE = false;
inline bool TrackLRU = true;


// SET THING ON,OFF
inline void runImmediateSET(std::deque<ForthToken> &tokens) {
    // we arrived here from TOKEN SET

    const ForthToken second = tokens.front();
    tokens.erase(tokens.begin()); // Remove the processed token
    if (tokens.empty()) return; // Exit early if no tokens to process
    auto feature = second.value;

    const ForthToken third = tokens.front();
    tokens.erase(tokens.begin()); // Remove the processed token
    if (tokens.empty()) return; // Exit early if no tokens to process
    auto state = third.value;

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
