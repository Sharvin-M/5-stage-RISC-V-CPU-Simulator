# RV32I Pipelined Instruction Set Simulator

A functional-level RISC-V simulator written in C++, designed to emulate the RV32I base integer instruction set. 

## Current Status: Pipelined Architecture
The simulator currently implements a **5-stage pipelined execution flow** for a subset of the RV32I ISA. It handles instruction fetching from text-based binary files and manages a virtualized register file and data memory. 

### Core Features
* **5-Stage Pipeline:** Fetch, Decode, Execute, Memory, Writeback.
* **Hazard Unit:** Fully implements load-use hazard detection and pipeline stalling.
* **Data Forwarding:** Eliminates data hazards by bypassing stale register data, routing ALU and Memory outputs directly back into the Execute stage.
* **Control Flushing:** Detects taken branches and jumps (`jal`, `jalr`), accurately flushing stale instructions from the Fetch and Decode latches.
* **Cycle-by-Cycle Trace:** Features a built-in pipeline visualization tool to track instruction flow and pipeline state (bubbles, stalls, forwards) on every clock cycle.

### Supported Instructions
- **Arithmetic/Logic:** `add`, `sub`, `and`, `or`, `addi`, `andi`, `ori`
- **Memory:** `lw`, `sw`
- **Control Flow:** `beq`, `bne`, `jal`, `jalr`

## Planned Expansion
The primary focus of ongoing development is expanding instruction support to reach full RV32I compliance, alongside memory hierarchy simulation.

- [ ] **Shift Operations:** `sll`, `srl`, `sra` (and immediate versions).
- [ ] **Comparisons:** `slt`, `sltu` and all branch variants (`blt`, `bge`).
- [ ] **U-Type Support:** `lui` and `auipc` for handling 20-bit immediates.
- [ ] **Cache Simulation:** Implementing L1 data/instruction caches to simulate memory latency and cache misses.

## Usage
Compile the simulator using a modern C++ compiler (requires C++11 or higher):

```bash
g++ pipeline_sim.cpp -o rv32i_sim
./rv32i_sim instructions.txt
```
