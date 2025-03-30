#ifndef JITCONTEXT_H
#define JITCONTEXT_H

#include <iostream>
#include <cstdio>
#include <ForthDictionary.h>

#include "asmjit/asmjit.h"
#include "Singleton.h"
#include "SignalHandler.h"

typedef void (*ForthFunction)();

typedef long (*ForthFunctionInt)(long);


class JitContext final : public Singleton<JitContext> {
    friend class Singleton<JitContext>;

public:
    [[nodiscard]] asmjit::x86::Assembler &getAssembler() {
        std::lock_guard<std::mutex> lock(init_mutex);
        if (!_assembler) {
            // Ensure _assembler is valid
            SignalHandler::instance().raise(10);
        }
        return *_assembler;
    }

    [[nodiscard]] asmjit::CodeHolder &getCode() {
        return _code;
    }

    // Enable logging with configurable options
    void enableLogging(bool logMachineCode = true, bool logAddresses = true) {
        _logger.setFile(stderr); // Default to stderr
        _logger.clearFlags(asmjit::FormatFlags::kMachineCode);
        _logger.clearFlags(asmjit::FormatFlags::kHexOffsets);
        _logger.clearFlags(asmjit::FormatFlags::kHexImms);
        if (logMachineCode) _logger.addFlags(asmjit::FormatFlags::kMachineCode);
        if (logAddresses) _logger.addFlags(asmjit::FormatFlags::kHexOffsets);
        _logger.addFlags(asmjit::FormatFlags::kHexImms);

        _code.setLogger(&_logger);
    }

    // Redirect logging to a file
    bool enableLoggingToFile(const std::string &filename) {
        FILE *file = fopen(filename.c_str(), "w");
        if (!file) {
            std::cerr << "JITContext: Failed to open log file: " << filename << std::endl;
            return false;
        }
        _logger.setFile(file);
        _code.setLogger(&_logger);
        _logFile = file;
        return true;
    }

    // Disable logging
    void disableLogging() {
        _code.setLogger(nullptr);
        if (_logFile) {
            if (fclose(_logFile) != 0) {
                std::cerr << "Failed to close log file." << std::endl;
            }
            _logFile = nullptr;
        }
    }

    void reportMemoryUsage() const {
        auto sectionCount = _code.sectionCount();
        for (size_t i = 0; i < sectionCount; ++i) {
            const asmjit::Section *section = _code.sectionById(i);
            if (!section) continue;
            std::cout << "Latest Word: " << ForthDictionary::instance().getLatestName() << std::dec << std::endl;
            const asmjit::CodeBuffer &buffer = section->buffer();
            std::cout << "Section " << i << ": " << section->name() << std::endl;
            std::cout << "  Buffer size    : " << buffer.size() << " bytes" << std::endl;
            std::cout << "  Real size    : " << section->realSize() << " bytes" << std::endl;
            std::cout << "  Virtual size    : " << section->virtualSize()   << " bytes" << std::endl;
            std::cout << "  Buffer capacity: " << buffer.capacity() << " bytes" << std::endl;
        }
    }

 void displayAsmJitMemoryUsage() const {
    auto toKB = [](size_t bytes) { return bytes / 1024.0; };

    std::cout << "AsmJit Memory Usage Metrics:" << std::endl;
    std::cout << "    Used:       " << toKB(_rt.allocator()->statistics().usedSize()) << " KB" << std::endl;
    std::cout << "    Reserved:   " << toKB(_rt.allocator()->statistics().reservedSize()) << " KB" << std::endl;
    std::cout << "    Overhead:   " << toKB(_rt.allocator()->statistics().overheadSize()) << " KB" << std::endl;
    std::cout << "    Allocation Count: "
              << _rt.allocator()->statistics().allocationCount() << " allocations" << std::endl;
}



    void initialize() {
        std::lock_guard<std::mutex> lock(init_mutex);
        delete _assembler;
        _assembler = nullptr;
        _code.reset();
        auto status = _code.init(_rt.environment());
        if (status != asmjit::kErrorOk) {
            SignalHandler::instance().raise(20);
        }
        _assembler = new asmjit::x86::Assembler(&_code);

    }

    ForthFunction finalize() {
        void *funcPtr = nullptr;
        asmjit::Error err = _rt.add(&funcPtr, &_code);
        if (err) {
            std::cerr << "Failed to finalize function: "
                    << asmjit::DebugUtils::errorAsString(err) << std::endl;
            return nullptr;
        }
        return reinterpret_cast<ForthFunction>(funcPtr);
    }

private:
    JitContext() {
        _logger.setFile(stderr); // Default logging to stderr
    }

public:

    asmjit::FileLogger _logger; // Logs assembly output
    asmjit::JitRuntime _rt; // JIT runtime for executable memory
    asmjit::CodeHolder _code; // Holds JIT-generated code
    asmjit::x86::Assembler *_assembler = nullptr; // Assembler for x86-64 instructions
    FILE *_logFile = nullptr; // File pointer for logging output
    std::mutex init_mutex;
};

#endif // JITCONTEXT_H
