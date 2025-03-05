#ifndef FORTHSYSTEM_H
#define FORTHSYSTEM_H

#include "LineReader.h"
#include "Tokenizer.h"
#include "Interpreter.h"
#include "Compiler.h"
#include "CodeGenerator.h"
#include "JitContext.h"
#include "DataStack.h"
#include "ReturnStack.h"
#include "LocalsHandler.h"
#include "VariableStorage.h"
#include "StringsStorage.h"
#include "SignalHandler.h"

class ForthSystem {
public:
    static ForthSystem& instance() {
        static ForthSystem instance;
        return instance;
    }

    static void initialize();

private:
    ForthSystem() = default;
    ~ForthSystem() = default;
    ForthSystem(const ForthSystem&) = delete;
    ForthSystem& operator=(const ForthSystem&) = delete;
};

#endif // FORTHSYSTEM_H
