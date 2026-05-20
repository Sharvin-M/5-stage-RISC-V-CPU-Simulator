# RV32I Instruction Set Simulator

A functional-level RISC-V simulator written in C, designed to emulate the RV32I base integer instruction set. 

## Current Status: Core Implementation
The simulator currently supports a single-cycle execution flow for a subset of the RV32I ISA. It handles instruction fetching from text-based "binary" files and manages a virtualized register file and data memory.

### Supported Instructions
- **Arithmetic/Logic:** `add`, `sub`, `and`, `or`, `addi`, `andi`, `ori`
- **Memory:** `lw`, `sw`
- **Control Flow:** `beq`, `jal`, `jalr`

##  Architecture
The project is structured around the classic RISC pipeline stages:
1. **Fetch:** Retrieves 32-bit instructions from an input file.
2. **Decode:** Extracts opcodes and operands while the **Control Unit** generates necessary signals.
3. **Execute:** Utilizes an ALU to perform operations and calculate branch targets.
4. **Memory:** Interfaces with a 32-word data memory array.
5. **Writeback:** Commits results to the register file.

##  Planned Expansion
The primary focus of ongoing development is expanding the instruction support to reach full RV32I compliance. 

- [ ] **Shift Operations:** `sll`, `srl`, `sra` (and immediate versions).
- [ ] **Comparisons:** `slt`, `sltu` and all branch variants (`bne`, `blt`, `bge`).
- [ ] **U-Type Support:** `lui` and `auipc` for handling 20-bit immediates.
- [ ] **Pipeline Logic:** Moving from single-cycle to a clocked, 5-stage pipeline with hazard handling.

## Usage
Compile the simulator using GCC:
```bash
gcc sim.c -o rv32i_sim
./rv32i_sim your_program.txt
