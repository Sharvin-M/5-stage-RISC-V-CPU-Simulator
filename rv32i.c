#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// CPU State
uint32_t pc = 0;
uint32_t next_pc = 4;
unsigned long clock_cycle = 0;
uint32_t data_memory[32] = {0};
unsigned int register_file[32] = {0};

// Buffer for Control Signals
typedef struct {
    char reg_write;
    char branch;
    char jump;
    char alu_src;
    char alu_op;
    char mem_write;
    char mem_to_reg;
    char mem_read;
    char alu_ctrl;
} ControlSignals;

// IF/ID Pipeline Register
typedef struct {
    char valid; // 0 if empty/bubble, 1 if holds valid instruction
    uint32_t pc;
    uint32_t next_pc;
    char char_instr[33];
    uint32_t int_instr;
} IF_ID;

// ID/EX Pipeline Register
typedef struct {
    int valid;
    uint32_t pc;
    uint32_t next_pc;
    int opcode;
    int funct3;
    int funct7;
    int rs1;
    int rs2;
    int rd;
    int rs1_val;
    int rs2_val;
    int imm;
    char name[8];
    ControlSignals ctrl;
} ID_EX;

// EX/MEM Pipeline Register
typedef struct {
    int valid;
    uint32_t pc;
    uint32_t next_pc;
    int alu_result;
    int alu_zero;
    int branch_target;
    int take_branch;
    int rs2_val; // Data to store in memory
    int rd;
    ControlSignals ctrl;
} EX_MEM;

// MEM/WB Pipeline Register
typedef struct {
    int valid;
    uint32_t next_pc;
    int mem_read_data;
    int alu_result;
    int rd;
    ControlSignals ctrl;
} MEM_WB;

// Global Pipeline Registers (Current state and Next state for clock edge simulation)
IF_ID if_id, next_if_id;
ID_EX id_ex, next_id_ex;
EX_MEM ex_mem, next_ex_mem;
MEM_WB mem_wb, next_mem_wb;

/* -----------------------------------------------------------------------------
 * PIPELINE STAGE FUNCTIONS
 * ---------------------------------------------------------------------------*/

void d_mem_init (uint32_t addr , uint32_t data) {
    addr /= 4;
    data_memory[addr] = data;
}

void Fetch(FILE *program) {
    unsigned int target_idx = pc / 4;
    unsigned int curr_idx = 0;
    char line[33];
    
    rewind(program);
    next_if_id.valid = 0;

    while (fgets(line, sizeof(line), program) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        if (curr_idx == target_idx) {
            strncpy(next_if_id.char_instr, line, sizeof(next_if_id.char_instr) - 1);
            next_if_id.char_instr[sizeof(next_if_id.char_instr) - 1] = '\0';
            next_if_id.int_instr = (uint32_t)strtoul(next_if_id.char_instr, NULL, 2);
            next_if_id.pc = pc;
            next_if_id.next_pc = pc + 4;
            next_if_id.valid = 1;
            
            pc = pc + 4; // Speculative PC update
            return;
        }
        curr_idx++;
    }
}

void ControlUnit(int opcode, int funct3, int funct7, ControlSignals *ctrl) {
    switch(opcode){
        case 3: { // lw
            ctrl->reg_write = 1; ctrl->branch = 0; ctrl->jump = 0; ctrl->alu_src = 1; 
            ctrl->alu_op = 0; ctrl->mem_write = 0; ctrl->mem_to_reg = 1; ctrl->mem_read = 1;
            break;
        }
        case 19: { // I-type 
            ctrl->reg_write = 1; ctrl->branch = 0; ctrl->jump = 0; ctrl->alu_src = 1; 
            ctrl->alu_op = 2; ctrl->mem_write = 0; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
        case 35: { // sw
            ctrl->reg_write = 0; ctrl->branch = 0; ctrl->jump = 0; ctrl->alu_src = 1; 
            ctrl->alu_op = 0; ctrl->mem_write = 1; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
        case 51: { // R-type 
            ctrl->reg_write = 1; ctrl->branch = 0; ctrl->jump = 0; ctrl->alu_src = 0; 
            ctrl->alu_op = 2; ctrl->mem_write = 0; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
        case 99: { // branch
            ctrl->reg_write = 0; ctrl->branch = 1; ctrl->jump = 0; ctrl->alu_src = 0; 
            ctrl->alu_op = 1; ctrl->mem_write = 0; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
        case 111: { // jal
            ctrl->reg_write = 1; ctrl->branch = 0; ctrl->jump = 1; ctrl->alu_src = 0; 
            ctrl->alu_op = 0; ctrl->mem_write = 0; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
        case 103: { // jalr
            ctrl->reg_write = 1; ctrl->branch = 0; ctrl->jump = 1; ctrl->alu_src = 1; 
            ctrl->alu_op = 2; ctrl->mem_write = 0; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
        default: {
            ctrl->reg_write = 0; ctrl->branch = 0; ctrl->jump = 0; ctrl->alu_src = 0; 
            ctrl->alu_op = 0; ctrl->mem_write = 0; ctrl->mem_to_reg = 0; ctrl->mem_read = 0;
            break;
        }
    }

    switch(ctrl->alu_op) {
        case 0: ctrl->alu_ctrl = 2; break; // ADD (Loads / Stores / Jumps)
        case 1: ctrl->alu_ctrl = 6; break; // SUB (Branches)
        case 2: {
            if (opcode == 19) {
                if (funct3 == 0) ctrl->alu_ctrl = 2; // addi
                if (funct3 == 7) ctrl->alu_ctrl = 0; // andi
                if (funct3 == 6) ctrl->alu_ctrl = 1; // ori
            } 
            else if (opcode == 51) {
                if (funct3 == 0 && funct7 == 0)  ctrl->alu_ctrl = 2; // add
                if (funct3 == 0 && funct7 == 32) ctrl->alu_ctrl = 6; // sub
                if (funct3 == 7 && funct7 == 0)  ctrl->alu_ctrl = 0; // and
                if (funct3 == 6 && funct7 == 0)  ctrl->alu_ctrl = 1; // or
            }
            break;
        }
    }
}

void Decode() {
    next_id_ex.valid = if_id.valid;
    if (!if_id.valid) return;

    uint32_t int_instr = if_id.int_instr;

    next_id_ex.opcode = int_instr & 0x7F;             
    next_id_ex.rd     = (int_instr >> 7) & 0x1F;      
    next_id_ex.funct3 = (int_instr >> 12) & 0x07;     
    next_id_ex.rs1    = (int_instr >> 15) & 0x1F;     
    next_id_ex.rs2    = (int_instr >> 20) & 0x1F;     
    next_id_ex.funct7 = (int_instr >> 25) & 0x7F;     

    next_id_ex.rs1_val = register_file
[next_id_ex.rs1];
    next_id_ex.rs2_val = register_file
[next_id_ex.rs2];
    next_id_ex.pc = if_id.pc;
    next_id_ex.next_pc = if_id.next_pc;

    int opcode = next_id_ex.opcode;
    const char* op_name = "unknown";
    int imm = 0;

    if (opcode == 0x33 || opcode == 0x3B) { // R-Type
        imm = 0; 
    }
    else if (opcode == 0x67 || opcode == 0x03 || opcode == 0x13) { // I-Type
        imm = (int32_t)int_instr >> 20;
    }
    else if (opcode == 0x23) { // S-Type
        imm = ((int32_t)(int_instr & 0xFE000000) >> 20) | ((int_instr >> 7) & 0x1F);
    }
    else if (opcode == 0x63) { // B-Type
        imm = ((int32_t)(int_instr & 0x80000000) >> 19) | 
              ((int_instr & 0x00000080) << 4)  | 
              ((int_instr & 0x7E000000) >> 20) | 
              ((int_instr & 0x00000F00) >> 7);   
    }
    else if (opcode == 0x17 || opcode == 0x37) { // U-Type
        imm = int_instr & 0xFFFFF000;
    }
    else if (opcode == 0x6F) { // J-Type
        imm = ((int32_t)(int_instr & 0x80000000) >> 11) | 
              (int_instr & 0x000FF000)         | 
              ((int_instr & 0x00100000) >> 9)  | 
              ((int_instr & 0x7FE00000) >> 20);  
    }
    
    next_id_ex.imm = imm;
    ControlUnit(opcode, next_id_ex.funct3, next_id_ex.funct7, &next_id_ex.ctrl);
}

void Execute() {
    next_ex_mem.valid = id_ex.valid;
    if (!id_ex.valid) return;

    int operandA = id_ex.rs1_val;
    int operandB = (id_ex.ctrl.alu_src == 1) ? id_ex.imm : id_ex.rs2_val;

    switch (id_ex.ctrl.alu_ctrl) {
        case 0:  next_ex_mem.alu_result = operandA & operandB;  break;  // AND
        case 1:  next_ex_mem.alu_result = operandA | operandB;  break;  // OR
        case 2:  next_ex_mem.alu_result = operandA + operandB;  break;  // ADD
        case 6:  next_ex_mem.alu_result = operandA - operandB;  break;  // SUB
        case 7:  next_ex_mem.alu_result = (operandA < operandB) ? 1 : 0; break; // SLT
        default: next_ex_mem.alu_result = 0; break;
    }

    next_ex_mem.alu_zero = (next_ex_mem.alu_result == 0) ? 1 : 0;

    int take_branch = 0;
    if (id_ex.ctrl.branch) {
        if (id_ex.funct3 == 0x0 && next_ex_mem.alu_zero) take_branch = 1; // beq
        if (id_ex.funct3 == 0x1 && !next_ex_mem.alu_zero) take_branch = 1; // bne
    }

    if (id_ex.ctrl.jump || take_branch) {
        next_ex_mem.take_branch = 1;
        if (id_ex.ctrl.jump && id_ex.opcode == 103) {
            next_ex_mem.branch_target = (id_ex.rs1_val + id_ex.imm) & ~1;  // jalr
        } else {
            next_ex_mem.branch_target = id_ex.pc + id_ex.imm; // jal and branches
        }
    } else {
        next_ex_mem.take_branch = 0;
    }

    // Pass along needed data
    next_ex_mem.rs2_val = id_ex.rs2_val;
    next_ex_mem.rd = id_ex.rd;
    next_ex_mem.ctrl = id_ex.ctrl;
    next_ex_mem.pc = id_ex.pc;
    next_ex_mem.next_pc = id_ex.next_pc;
}

void Mem() {
    next_mem_wb.valid = ex_mem.valid;
    if (!ex_mem.valid) return;

    int idx = ex_mem.alu_result / 4; 
    next_mem_wb.mem_read_data = 0;

    if (ex_mem.ctrl.mem_write == 1) {
        data_memory[idx] = ex_mem.rs2_val;
        printf("total_clock_cycles %lu :\nmemory 0x%x is modified to 0x%x\n", clock_cycle, ex_mem.alu_result, ex_mem.rs2_val);
    }
    if (ex_mem.ctrl.mem_read == 1) {
        next_mem_wb.mem_read_data = data_memory[idx];
    }

    // Handle PC updates for branches/jumps in memory stage (simplified resolving)
    if (ex_mem.take_branch) {
        pc = ex_mem.branch_target;
        printf("pc is modified to 0x%x (Branch/Jump Taken)\n", pc);
        // Note: In a full pipeline, you must flush IF/ID and ID/EX here.
    }

    next_mem_wb.alu_result = ex_mem.alu_result;
    next_mem_wb.rd = ex_mem.rd;
    next_mem_wb.ctrl = ex_mem.ctrl;
    next_mem_wb.next_pc = ex_mem.next_pc;
}

void Writeback() {
    if (!mem_wb.valid) return;

    if (mem_wb.ctrl.reg_write == 1 && mem_wb.rd != 0) {
        int write_data;
        if (mem_wb.ctrl.mem_to_reg == 1) write_data = mem_wb.mem_read_data;  
        else if (mem_wb.ctrl.jump)       write_data = mem_wb.next_pc; 
        else                             write_data = mem_wb.alu_result;     

        register_file
    [mem_wb.rd] = write_data;
        printf("total_clock_cycles %lu : x%d is modified to 0x%x\n", clock_cycle, mem_wb.rd, register_file
    [mem_wb.rd]);
    }
}

void UpdateRegisters()
{
    if_id = next_if_id;
    id_ex = next_id_ex;
    ex_mem = next_ex_mem;
    mem_wb = next_mem_wb;
}

int main(int argc, char **argv)
{

    register_file[1] = 0x20;
    register_file[2] = 0x5;
    register_file[10] = 0x70;
    register_file[11] = 0x4;

    d_mem_init(0x70, 0x5);
    d_mem_init(0x74, 0x10);

    if (argc < 2)
    {
        printf("Usage: %s <instruction_file>\n", argv[0]);
        return 1;
    }

    FILE *program = fopen(argv[1], "r");
    if (program == NULL)
    {
        perror("Error opening instruction file");
        return 1;
    }

    // Initialize latches to empty
    memset(&if_id, 0, sizeof(IF_ID));
    memset(&id_ex, 0, sizeof(ID_EX));
    memset(&ex_mem, 0, sizeof(EX_MEM));
    memset(&mem_wb, 0, sizeof(MEM_WB));

    while (1)
    {
        // Execute stages in reverse order to simulate concurrent hardware execution.
        // A stage reads from its current input buffer and writes to the 'next' output buffer.

        Writeback();
        Mem();
        Execute();
        Decode();
        Fetch(program);

        // If all stages are empty (pipeline drained), we terminate
        if (!next_if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid)
        {
            break;
        }

        // Clock cycle increments once all stages finish their work for that tick
        clock_cycle++;

        // Commit all "next" states into the "current" latches for the next clock edge
        UpdateRegisters();
    }

    fclose(program);
    printf("program terminated\n");
    printf("total execution time is %lu cycles\n", clock_cycle);
    return 0;
}