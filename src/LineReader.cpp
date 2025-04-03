#include "LineReader.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <termios.h>

struct termios t;
// Enable raw mode

inline void enable_raw_mode(struct termios *orig_termios) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig_termios);
    raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
    raw.c_cc[VMIN] = 1; // Min characters to read
    raw.c_cc[VTIME] = 0; // No timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

}

// Restore original terminal settings
void disable_raw_mode(struct termios *orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

void LineReader::initialize() {
    enable_raw_mode(&t);
}

void LineReader::finalize() {
    disable_raw_mode(&t);
}


#define MAX_HISTORY 50  // Maximum history size

std::vector<std::string> history; // Command history buffer
int history_index = -1; // Index for navigating history

inline void read_input_c(char *buffer, size_t max_length) {
    size_t pos = 0; // Current cursor position
    size_t length = 0; // Length of the input string in `buffer`
    char c;
    std::string current_input; // Keeps track of the current input for history navigation

    while (true) {
        if (read(STDIN_FILENO, &c, 1) != 1) break; // Read a single character


        // Handle Enter (complete the line input)
        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            buffer[length] = '\0'; // Null-terminate the input

            if (length > 0) {
                history.push_back(std::string(buffer)); // Save input to history
                if (history.size() > MAX_HISTORY) history.erase(history.begin()); // Limit history
            }

            history_index = -1; // Reset history navigation
            break;
        }

        // Handle Backspace
        if (c == 127 || c == 8) {
            // Backspace or Ctrl-H
            if (pos > 0) {
                pos--;
                length--;
                memmove(buffer + pos, buffer + pos + 1, length - pos); // Remove the char at `pos`
                buffer[length] = '\0';

                // Update screen
                write(STDOUT_FILENO, "\b", 1); // Move cursor back
                write(STDOUT_FILENO, &buffer[pos], length - pos); // Redraw the rest of the line
                write(STDOUT_FILENO, " ", 1); // Erase extra character on screen
                write(STDOUT_FILENO, "\b", 1); // Move cursor back one space
                for (size_t i = pos; i < length; i++) write(STDOUT_FILENO, "\b", 1); // Place cursor correctly
            }
            continue;
        }

        // Handle Arrow Keys
        if (c == 27) {
            // Escape sequence
            char seq[2];
            if (read(STDIN_FILENO, seq, 2) == 2) {
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': // Up - Previous history
                            if (!history.empty() && history_index + 1 < (int) history.size()) {
                                if (history_index == -1) current_input = std::string(buffer, length);
                                // Save current input
                                history_index++;
                                strncpy(buffer, history[history.size() - 1 - history_index].c_str(), max_length - 1);
                                length = strlen(buffer);
                                pos = length;
                                buffer[length] = '\0';
                                write(STDOUT_FILENO, "\33[2K\r>", 6); // Clear the line and move to the start
                                write(STDOUT_FILENO, buffer, length); // Display history item
                            }
                            continue;
                        case 'B': // Down - Next history
                            if (history_index > 0) {
                                history_index--;
                                strncpy(buffer, history[history.size() - 1 - history_index].c_str(), max_length - 1);
                            } else if (history_index == 0) {
                                history_index = -1;
                                strncpy(buffer, current_input.c_str(), max_length - 1);
                            }
                            length = strlen(buffer);
                            pos = length;
                            buffer[length] = '\0';
                            write(STDOUT_FILENO, "\33[2K\r", 4); // Clear line
                            write(STDOUT_FILENO, buffer, length); // Display history item
                            continue;
                        case 'D': // Left - Move cursor left
                            if (pos > 0) {
                                write(STDOUT_FILENO, "\033[D", 3); // Move cursor left
                                pos--;
                            }
                            continue;
                        case 'C': // Right - Move cursor right
                            if (pos < length) {
                                write(STDOUT_FILENO, "\033[C", 3); // Move cursor right
                                pos++;
                            }
                            continue;
                    }
                }
            }
            continue;
        }

        if (c == 1) {
            // Ctrl+A - Move to start of line
            while (pos > 0) {
                write(STDOUT_FILENO, "\033[D", 3); // Move left
                pos--;
            }
            continue;
        }

        if (c == 5) {
            // Ctrl+E - Move to end of line
            while (pos < length) {
                write(STDOUT_FILENO, "\033[C", 3); // Move right
                pos++;
            }
            continue;
        }

        // Handle Normal Character Input
        if (length < max_length - 1) {
            // Shift the buffer to the right starting from `pos`
            memmove(buffer + pos + 1, buffer + pos, length - pos);
            buffer[pos] = c; // Insert character at cursor
            length++;
            pos++;

            // Redraw updated input to the screen
            write(STDOUT_FILENO, &buffer[pos - 1], length - pos + 1); // Write rest of the string
            for (size_t i = pos; i < length; i++) write(STDOUT_FILENO, "\b", 1); // Move cursor back to correct position
        }
    }
}

// Wrapper function to replace std::getline with read_input_c
inline std::istream &custom_getline(std::istream &input, std::string &line) {
    constexpr size_t MAX_INPUT_LENGTH = 1024;
    char buffer[MAX_INPUT_LENGTH];

    if (!input.good()) return input; // Handle stream errors

    // Use your custom terminal input function
    read_input_c(buffer, MAX_INPUT_LENGTH);

    // Convert buffer to std::string
    line = std::string(buffer);

    return input;
}

std::string LineReader::readLine() {
    std::string input;
    custom_getline(std::cin, input);
    return input;
}
