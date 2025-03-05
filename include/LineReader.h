#ifndef LINEREADER_H
#define LINEREADER_H

#include "Singleton.h"
#include <string>

class LineReader : public Singleton<LineReader> {
    friend class Singleton<LineReader>;

public:

    static std::string readLine();
    static void initialize();
    static void finalize();

private:
    LineReader() = default;
};

#endif // LINEREADER_H
