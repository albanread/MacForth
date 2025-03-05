#ifndef CASESTATEMENTMANAGER_H
#define CASESTATEMENTMANAGER_H

#include <stack>
#include <vector>
#include <string>
#include "asmjit/asmjit.h"
#include "LabelManager.h"

class CaseStatementManager {
public:
    explicit CaseStatementManager(asmjit::x86::Assembler &assembler)
        : _assembler(assembler), _labelManager(assembler) {}

    // Start a new CASE structure
    void beginCase() {
        _caseEndLabel = _labelManager.createLabel("case_end");
        _caseStack.push({}); // Push a new `OF` block set
    }

    // Add a new OF block (value to compare)
    void addOfBlock(int64_t compareValue) {
        if (_caseStack.empty()) {
            throw std::runtime_error("CaseStatementManager: `OF` without `CASE`.");
        }

        // Create labels for false and the execution block
        asmjit::Label falseLabel = _labelManager.createLabel("case_of_false");
        asmjit::Label execLabel = _labelManager.createLabel("case_exec");

        // Compare TOS (assumed in `rax`) with the given value
        _assembler.cmp(asmjit::x86::rax, compareValue);
        _assembler.jne(falseLabel); // If not equal, jump to next `OF`

        // Bind execution label (this runs if the value matched)
        _labelManager.bindLabel("case_exec");

        // Store label for `ENDOF`
        _caseStack.top().push_back(falseLabel);
    }

    // End an `OF` block (jump to `ENDCASE`)
    void endOfBlock() {
        if (_caseStack.empty() || _caseStack.top().empty()) {
            throw std::runtime_error("CaseStatementManager: `ENDOF` without `OF`.");
        }

        // Jump to ENDCASE
        _assembler.jmp(_caseEndLabel);

        // Bind false label (execution falls here if `OF` was false)
        _labelManager.bindLabel("case_of_false");
    }

    // Define the default case (only one per CASE)
    void addDefaultBlock() {
        if (!_defaultLabel.isValid()) {
            _defaultLabel = _labelManager.createLabel("case_default");
        }
        _labelManager.bindLabel("case_default");
    }

    // End the CASE structure
    void endCase() {
        if (_caseStack.empty()) {
            throw std::runtime_error("CaseStatementManager: `ENDCASE` without `CASE`.");
        }

        // Bind all pending `OF` false branches to default or ENDCASE
        for (asmjit::Label falseLabel : _caseStack.top()) {
            _labelManager.bindLabel("case_of_false");
        }
        _caseStack.pop();

        // Bind ENDCASE label
        _labelManager.bindLabel("case_end");
    }

private:
    asmjit::x86::Assembler &_assembler;
    LabelManager _labelManager;
    std::stack<std::vector<asmjit::Label>> _caseStack;
    asmjit::Label _caseEndLabel;
    asmjit::Label _defaultLabel;
};

#endif // CASESTATEMENTMANAGER_H
