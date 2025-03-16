// NOLINT BEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult, clang-diagnostic-type-limits)
#ifndef REGISTER_TRACKER_H
#define REGISTER_TRACKER_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <list>
#include <set>
#include <asmjit/asmjit.h>
#include "Singleton.h"
#include "SignalHandler.h"
#include "Settings.h"
#include <cstddef>   // For std::byte
#include <thread>    // For thread-local variables
#include "CodeGenerator.h"

// Thread-local spill slot memory
static thread_local std::vector<std::byte> gSpillSlotMemory;
static thread_local bool gSpillMemoryInitialized = false;

static constexpr int CACHE_REG_R12 = 12;
static constexpr int CACHE_REG_R13 = 13;
static constexpr int CACHE_REG_R14 = 14;
static constexpr int CACHE_REG_R15 = 15;
static constexpr int SPILL_ALIGNMENT = 16;
static constexpr int MAX_SPILL_SLOTS = 1000;


class RegisterTracker : public Singleton<RegisterTracker> {
public:
    using Xmm = int;
    using Gp = int;
    friend class Singleton<RegisterTracker>; // Allow `Singleton` to access the constructor

public:
    static constexpr int NUM_XMM_REGS = 16; // xmm0 - xmm15
    static constexpr int NUM_GP_CACHE_REGS = 4; // R12, R13, R14, R15
    static constexpr int GLOBAL_MEMORY_OFFSET = 0x100; // Base offset for spills

    RegisterTracker()
        : spillOffset(0) {
        for (int i = 0; i < NUM_XMM_REGS; ++i) {
            freeXmmRegisters.push_back(i);
        }
        freeGpCache = {
            {"r12", CACHE_REG_R12},
            {"r13", CACHE_REG_R13},
            {"r14", CACHE_REG_R14},
            {"r15", CACHE_REG_R15}
        };
    }


    /** Initialize or reset the tracker */
    void initialize() {
        // Clear and reinitialize all internal states
        registerMap.clear();
        spillSlots.clear();
        registerUsage.clear();
        freeXmmRegisters.clear();
        reservedXmmRegisters.clear();
        freeGpCache.clear();
        constantValues.clear();
        cacheToGP = false;

        ensureThreadLocalSpillMemory(MAX_SPILL_SLOTS);
        // Reinitialize spill offset
        spillOffset = 0;

        base_slots = getThreadLocalSpillMemory();

        // Populate free XMM registers
        for (int i = 0; i < NUM_XMM_REGS; ++i) {
            freeXmmRegisters.push_back(i);
        }

        // Reinitialize the general-purpose register cache
        freeGpCache = {
            {"r12", CACHE_REG_R12},
            {"r13", CACHE_REG_R13},
            {"r14", CACHE_REG_R14},
            {"r15", CACHE_REG_R15}
        };

        debugMessage("RegisterTracker initialized successfully.");
    }


    static void ensureThreadLocalSpillMemory(size_t numSlots) {
        if (!gSpillMemoryInitialized) {
            initializeThreadLocalSpillMemory(numSlots);
            gSpillMemoryInitialized = true;
        }
    }

    // Initializes thread-local memory for N spill slots (each 16 bytes)
    static void initializeThreadLocalSpillMemory(size_t numSlots) {
        size_t totalSize = numSlots * 16; // Calculate total memory size
        gSpillSlotMemory.resize(totalSize); // Resize thread-local buffer
    }

    // Retrieves a pointer to the start of the thread-local spill memory
    static void *getThreadLocalSpillMemory() {
        return gSpillSlotMemory.data();
    }

    static void *getSpillSlotBase() {
        return gSpillSlotMemory.data();
    }

    int allocateFreeXmmRegister() {
        while (!freeXmmRegisters.empty()) {
            int regId = freeXmmRegisters.back();
            freeXmmRegisters.pop_back();
            if (reservedXmmRegisters.find(regId) == reservedXmmRegisters.end()) {
                return regId;
            }
        }
        std::cerr << ("No free XMM registers available");
        SignalHandler::instance().raise(25);
        return -1;
    }

    void freeXmmRegister(int regId) {
        if (reservedXmmRegisters.find(regId) == reservedXmmRegisters.end()) {
            freeXmmRegisters.push_back(regId);
        }
    }


    static asmjit::x86::Xmm createXmmFromId(int id) {
        return asmjit::x86::xmm(id);
    }

    asmjit::x86::Xmm createXmmFromVarName(const std::string &varName) {



        auto it = registerMap.find(varName);
        if (it == registerMap.end()) {
            std::cerr << ("Variable not found in registerMap: " + varName);
            SignalHandler::instance().raise(25);
        }
        return asmjit::x86::xmm(it->second); // Create Xmm register from the stored ID
    }

    asmjit::x86::Gp createGPFromVarName(const std::string &varName) {
        const auto it = registerMap.find(varName);
        if (it == registerMap.end()) {
            std::cerr << ("Variable not found in registerMap: " + varName);
            SignalHandler::instance().raise(25);
        }

        // Use asmjit::x86::gpq (for 64-bit registers) to create a Gp register from an ID.
        // Replace gpq with gpd or gpw if you require 32-bit or 16-bit registers.
        return asmjit::x86::gpq(it->second);
    }


    asmjit::x86::Xmm allocateRegister(const std::string &varName) {

        // Check if the variable already has a register allocated

        auto it = registerMap.find(varName);
        if (it != registerMap.end()) {
            registerAccessCounter[varName]++; // Update LRU metric
            debugMessage("Reusing register " + xmmRegToStr(it->second) + " for " + varName);
            return createXmmFromId(it->second);
        }

        // Allocate from free registers
        while (!freeXmmRegisters.empty()) {
            int regId = freeXmmRegisters.back(); // Get the last free register
            freeXmmRegisters.pop_back(); // Remove it from the free list

            // Skip reserved registers
            if (reservedXmmRegisters.find(regId) != reservedXmmRegisters.end()) {
                continue;
            }

            // Allocate the register
            registerMap[varName] = regId; // Map variable to the register
            registerUsage.push_front(varName); // Track usage for spilling
            debugMessage("Allocated register " + xmmRegToStr(regId) + " for " + varName);
            registerAccessCounter[varName] = 1; // new LRU metric
            return createXmmFromId(regId);
        }

        // No free registers left, spill one and retry allocation
        return spillRegister(varName);
    }


    /** Frees a register when the value is no longer needed (does NOT affect reserved registers) */
    void freeRegister(const std::string &varName) {
        auto it = registerMap.find(varName);
        if (it != registerMap.end()) {
            asmjit::x86::Xmm reg = createXmmFromId(it->second);

            if (reservedXmmRegisters.find(reg.id()) != reservedXmmRegisters.end()) {
                debugMessage(
                    "Attempted to free reserved register " + xmmRegToStr(reg) + " for " + varName + " (ignored).");
                return;
            }

            freeXmmRegisters.push_back(reg.id());
            registerMap.erase(it);
            registerUsage.remove(varName);
            registerAccessCounter.erase(varName); // Remove LRU metric
            debugMessage("Freed register " + xmmRegToStr(reg) + " from " + varName);
        }
    }


    // save for C call
    void spillRegisters() {
        asmjit::x86::Assembler *assembler;
        initialize_assembler(assembler);

        // Define the registers to check for spilling (xmm3 to xmm7)
        std::vector<int> registersToSpill = {3, 4, 5, 6, 7};

        for (int regId: registersToSpill) {
            // Check if the register is currently allocated
            auto it = std::find_if(registerMap.begin(), registerMap.end(),
                                   [regId](const auto &pair) {
                                       return pair.second == regId;
                                   });

            if (it != registerMap.end()) {
                // This register is in use, spill it to memory
                const std::string &varName = it->first; // Get the variable name using this register

                // Check if already spilled
                if (spillSlots.find(varName) == spillSlots.end()) {
                    debugMessage("Spilling register xmm" + std::to_string(regId) + " for variable: " + varName);

                    // Assign a memory offset for spilling
                    spillSlots[varName] = spillOffset;

                    // Generate assembly code to spill the register
                    //assembler->mov(asmjit::x86::rax, spillOffset); // Load spill offset into rax
                    assembler->movsd(asmjit::x86::ptr(asmjit::x86::rdi, spillOffset), createXmmFromId(regId));
                    spillOffset += SPILL_ALIGNMENT; // Move to the next aligned memory slot
                }
            }
        }
    }


    /** Spills a register into memory (constants are only spilled once) */
    asmjit::x86::Xmm spillRegister(const std::string &varName) {

        asmjit::x86::Assembler *assembler;
        initialize_assembler(assembler);

        if (registerUsage.empty()) {
            std::cerr << ("No registers available for spilling.");
            SignalHandler::instance().raise(25);
        }

        std::string spilledVar = registerUsage.back();
        registerUsage.pop_back();
        const Xmm spilledRegLU = registerMap[spilledVar];


        int minUsage = INT_MAX;  // Start with a high value
        for (const auto &entry : registerAccessCounter) {
            if (registerMap.find(entry.first) != registerMap.end() && entry.second < minUsage) {
                minUsage = entry.second;
                spilledVar = entry.first;
            }
        }
        const Xmm spilledRegLRU = registerMap[spilledVar];

        const Xmm spilledReg = (LRU) ? spilledRegLRU : spilledRegLU;
        debugMessage("Spilling using " + std::string(LRU ? "LRU" : "FIFO") + " strategy: " + spilledVar);


        if (reservedXmmRegisters.find(spilledReg) != reservedXmmRegisters.end()) {
            debugMessage("Skipping spill of reserved register " + xmmRegToStr(spilledReg));
            return allocateRegister(varName);
        }

        // If it's a constant and already spilled, no need to spill again
        if (constantValues.find(spilledVar) != constantValues.end()) {
            debugMessage("Skipping re-spill of constant value: " + spilledVar);
            return allocateRegister(varName);
        }

        // If the value is already in spillSlots, no need to spill it again
        if (spillSlots.find(spilledVar) != spillSlots.end()) {
            debugMessage("Skipping re-spill of " + spilledVar + " (already stored in memory).");
            return allocateRegister(varName);
        }

        if (isGpVarInCache(spilledVar)) {
            debugMessage("Skipping re-caching of spilledVar");
            return allocateRegister(varName);
        }

        if (cacheToGP && !freeGpCache.empty()) {
            gpCacheUsed = true;
            asmjit::x86::Gp gpReg = createGPFromVarName(varName); // Assign the register to spilledVar in the map.
            freeGpCache[spilledVar] = gpReg.id(); // Use unordered_map to store associations.

            assembler->movq(gpReg, createXmmFromId(spilledReg));
            debugMessage("Cached " + spilledVar + " into " + GPregToStr(gpReg));

            registerMap.erase(spilledVar);
            registerAccessCounter.erase(spilledVar);
            freeXmmRegister(spilledReg);

            return allocateRegister(varName);
        }


        // Move value to memory
        registerAccessCounter.erase(spilledVar);
        spillSlots[spilledVar] = spillOffset;
        //assembler->mov(asmjit::x86::rax, spillOffset);
        assembler->movsd(asmjit::x86::ptr(asmjit::x86::rdi, spillOffset), createXmmFromId(spilledReg));
        spillOffset += SPILL_ALIGNMENT; // Ensure 16-byte alignment

        registerMap.erase(spilledVar);
        freeXmmRegister(spilledReg);

        debugMessage("Spilled " + spilledVar + " from " + xmmRegToStr(spilledReg) + " to memory at offset " +
                     std::to_string(spillSlots[spilledVar]));

        return allocateRegister(varName);
    }


    /** Reloads a spilled variable back into a register */
    asmjit::x86::Xmm reloadFromSpill(const std::string &varName) {
        asmjit::x86::Assembler *assembler;
        initialize_assembler(assembler);

        if (spillSlots.find(varName) == spillSlots.end()) {
            std::cerr << ("Variable not found in spill slots: " + varName);
            SignalHandler::instance().raise(25);
        }

        asmjit::x86::Xmm reg = allocateRegister(varName);
        //assembler->mov(asmjit::x86::rax, spillSlots[varName]);
        assembler->movsd(reg, asmjit::x86::ptr(asmjit::x86::rdi, spillSlots[varName]));
        spillSlots.erase(varName);

        debugMessage("Reloaded " + varName + " from memory into " + xmmRegToStr(reg));
        return reg;
    }

    void printRegisterStatus() {
        debugMessage("=== Register Allocations ===");
        for (const auto &entry: registerMap) {
            debugMessage("Variable: " + entry.first + " -> Register: " + xmmRegToStr(entry.second));
        }
        debugMessage("Constant Values:");
        for (const auto &entry: constantValues) {
            debugMessage("Constant: " + entry);
        }
        debugMessage("Spilled to Memory:");
        for (const auto &entry: spillSlots) {
            debugMessage("Variable: " + entry.first + " -> Memory Offset: " + std::to_string(entry.second));
        }
        debugMessage("Cached General Purpose Registers:");
        for (const auto &entry: freeGpCache) {
            debugMessage("Variable: " + entry.first + " -> GP Register: " + GPregToStr(entry.second));
        }
        // New Debug Message: GP Cache Status
        debugMessage("GP Cache Usage: " + std::string(gpCacheUsed ? "Yes" : "No"));

        debugMessage("============================");
    }


    void clearConstant(const std::string &varName) {
        constantValues.erase(varName);
    }

    void setConstant(const std::string &varName) {
        constantValues.insert(varName);
    }

    [[nodiscard]] bool wasGpCacheUsed() const {
        return gpCacheUsed;
    }

    void enableGpCache() {
        cacheToGP = true;
    }

    void disableGpCache() {
        cacheToGP = false;
    }

    void enableLRU() {
        LRU = true;
    }

    void disableLRU() {
        LRU = false;
    }

    bool isAllocated(const std::string &name) {
        return (registerMap.find(name) != registerMap.end());
    }

    /** Converts an AsmJit register to a string */
    static std::string xmmRegToStr(const asmjit::x86::Xmm &reg) {
        return "xmm" + std::to_string(reg.id());
    }

    static std::string xmmRegToStr(const int r) {
        return "xmm" + std::to_string(r);
    }

private:
    bool gpCacheUsed = false;
    bool cacheToGP = false;
    bool LRU = false;
    std::unordered_map<std::string, int> registerMap;
    std::unordered_map<std::string, int> spillSlots;
    std::list<std::string> registerUsage;
    std::vector<int> freeXmmRegisters;
    std::set<int> reservedXmmRegisters; // Reserved XMM register IDs
    u_int64_t spillOffset;
    void *base_slots = nullptr;
    std::unordered_map<std::string, int> freeGpCache;
    std::unordered_set<std::string> constantValues;
    std::unordered_map<std::string, int> registerAccessCounter; // LRU metric

    // New map to track variables cached in GP registers
    std::unordered_map<std::string, int> gpCacheMap;

    /** Converts a GP register to a string */
    static std::string GPregToStr(const asmjit::x86::Gp &reg) {
        // Customize the string format for GP registers
        return "Gp" + std::to_string(reg.id());
    }

    static std::string GPregToStr(const int r) {
        // Customize the string format for GP registers
        return "Gp" + std::to_string(r);
    }

    /** Centralized debug message handler */
    static void debugMessage(const std::string &msg) {
        if (false) {
            std::cerr << "[DEBUG] " << msg << std::endl;
        }
    }

    /** Checks if a GP register is in the free cache */
    [[nodiscard]] bool isGpRegInCache(const asmjit::x86::Gp &reg) const {
        const int rid = reg.id();
        return std::any_of(freeGpCache.begin(), freeGpCache.end(), [rid](const auto &pair) {
            return pair.second == rid;
        });
    }


    [[nodiscard]] bool isGpRegInCache(const int rid) const {
        return std::any_of(freeGpCache.begin(), freeGpCache.end(), [rid](const auto &pair) {
            return pair.second == rid;
        });
    }

    /** Checks if a variable is already cached in a GP register */
    [[nodiscard]] bool isGpVarInCache(const std::string &varName) const {
        return gpCacheMap.find(varName) != gpCacheMap.end();
    }
};

#endif // REGISTER_TRACKER_H
// NOLINT END(clang-analyzer-core.UndefinedBinaryOperatorResult, clang-diagnostic-type-limits)
