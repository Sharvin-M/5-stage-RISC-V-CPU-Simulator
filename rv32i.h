#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Global State
uint32_t pc = 0;
uint32_t next_pc = 4;
uint32_t branch_target = 0;
uint32_t use_branch_target = 0;
unsigned long clock_cycle = 0;
uint32_t d_mem[32] = {0};
uint32_t mem_read_data = 0;
unsigned int rf[32] = {0};
char char_instr[33];

void d_mem_init (uint32_t addr , uint32_t data) {
    addr /= 4;
    d_mem[addr] = data;
}

int Fetch(FILE *program, char *instr, size_t instr_size) {
    unsigned int target_idx = pc / 4;
    unsigned int curr_idx = 0;
    char line[33];

    rewind(program);

    while (fgets(line, sizeof(line), program) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        if (curr_idx == target_idx) {
            strncpy(instr, line, instr_size - 1);
            instr[instr_size - 1] = '\0';
            next_pc = pc + 4;
            return 1;
        }
        curr_idx++;
    }
    return 0; 
}

// ALU outputs
int alu_zero = 0;
int alu_result = 0;

// Decoded instruction fields
int opcode   = 0;
int funct3   = 0;
int funct7   = 0;
int rs1      = 0;
int rs2      = 0;
int rd       = 0;
int rs1_val  = 0;
int rs2_val  = 0;
int imm      = 0;
int valid    = 0;
char name[8] = "";

/* Control signal values */
int reg_write = 0;
int branch   = 0;
int jump     = 0;
int alu_src   = 0;
int alu_op    = 0;
int mem_write = 0;
int mem_to_reg = 0;
int mem_read  = 0;
int alu_ctrl  = 0;

void Decode() {
    uint32_t int_instr = (uint32_t)strtoul(char_instr, NULL, 2);

    opcode = int_instr & 0x7F;             // Bits 0-6
    rd     = (int_instr >> 7) & 0x1F;      // Bits 7-11
    funct3 = (int_instr >> 12) & 0x07;     // Bits 12-14
    rs1    = (int_instr >> 15) & 0x1F;     // Bits 15-19
    rs2    = (int_instr >> 20) & 0x1F;     // Bits 20-24
    funct7 = (int_instr >> 25) & 0x7F;     // Bits 25-31

    rs1_val = rf[rs1];
    rs2_val = rf[rs2];
    valid = 1;

    const char* op_name = "unknown";

    if (opcode == 0x33 || opcode == 0x3B) { 
        switch (funct3) {
            case 0x0: op_name = (funct7 == 0x00) ? "add" : "sub"; break;
            case 0x1: op_name = "sll"; break;
            case 0x2: op_name = "slt"; break;
            case 0x3: op_name = "sltu"; break;
            case 0x4: op_name = "xor"; break;
            case 0x5: op_name = (funct7 == 0x00) ? "srl" : "sra"; break;
            case 0x6: op_name = "or"; break;
            case 0x7: op_name = "and"; break;
        }
        imm = 0; // R-type has no immediate
        printf("Instruction Type: R | Operation: %s\n", op_name);
    }

    else if (opcode == 0x67 || opcode == 0x03 || opcode == 0x13) { 
        // Sign-extend 12-bit immediate
        imm = (int32_t)int_instr >> 20;

        if (opcode == 0x13) { 
            switch (funct3) {
                case 0x0: op_name = "addi"; break;
                case 0x1: op_name = "slli"; break;
                case 0x2: op_name = "slti"; break;
                case 0x3: op_name = "sltiu"; break;
                case 0x4: op_name = "xori"; break;
                case 0x5: op_name = ((funct7 & 0x20) == 0) ? "srli" : "srai"; break;
                case 0x6: op_name = "ori"; break;
                case 0x7: op_name = "andi"; break;
            }
        } else if (opcode == 0x03) { 
            switch (funct3) {
                case 0x0: op_name = "lb"; break;
                case 0x1: op_name = "lh"; break;
                case 0x2: op_name = "lw"; break;
            }
        } else if (opcode == 0x67) {
            op_name = "jalr";
        }
        printf("Instruction Type: I | Operation: %s | Imm: %d\n", op_name, imm);
    }

    else if (opcode == 0x23) { 
        switch (funct3) {
            case 0x0: op_name = "sb"; break;
            case 0x1: op_name = "sh"; break;
            case 0x2: op_name = "sw"; break;
        }
        // Reconstruct S-Immediate
        imm = ((int32_t)(int_instr & 0xFE000000) >> 20) | ((int_instr >> 7) & 0x1F);
        printf("Instruction Type: S | Operation: %s | Imm: %d\n", op_name, imm);
    }

    // --- B-TYPE INSTRUCTIONS ---
    else if (opcode == 0x63) { 
        switch (funct3) {
            case 0x0: op_name = "beq"; break;
            case 0x1: op_name = "bne"; break;
            case 0x4: op_name = "blt"; break;
            case 0x5: op_name = "bge"; break;
        }
        // Reconstruct B-Immediate
        imm = ((int32_t)(int_instr & 0x80000000) >> 19) | 
                  ((int_instr & 0x00000080) << 4)  | 
                  ((int_instr & 0x7E000000) >> 20) | 
                  ((int_instr & 0x00000F00) >> 7);   
        printf("Instruction Type: B | Operation: %s | Imm: %d\n", op_name, imm);
    }

    // --- U-TYPE INSTRUCTIONS ---
    else if (opcode == 0x17 || opcode == 0x37) { 
        op_name = (opcode == 0x17) ? "auipc" : "lui";
        imm = int_instr & 0xFFFFF000;
        printf("Instruction Type: U | Operation: %s | Imm: 0x%08X\n", op_name, imm);
    }

    // --- J-TYPE INSTRUCTIONS ---
    else if (opcode == 0x6F) { 
        op_name = "jal";
        // Reconstruct J-Immediate
        imm = ((int32_t)(int_instr & 0x80000000) >> 11) | 
                  (int_instr & 0x000FF000)         | 
                  ((int_instr & 0x00100000) >> 9)  | 
                  ((int_instr & 0x7FE00000) >> 20);  
        printf("Instruction Type: J | Operation: %s | Imm: %d\n", op_name, imm);
    }
    else {
        valid = 0;
        printf("Unknown Opcode: 0x%02X\n", opcode);
    }

    strncpy(name, op_name, sizeof(name) - 1);
}

void ControlUnit(int opcode, int funct3, int funct7) {
    switch(opcode){
        case 3: { // lw
            reg_write = 1; branch = 0; jump = 0; alu_src = 1; alu_op = 0; mem_write = 0; mem_to_reg = 1; mem_read = 1;
            break;
        }
        case 19: { // I-type 
            reg_write = 1; branch = 0; jump = 0; alu_src = 1; alu_op = 2; mem_write = 0; mem_to_reg = 0; mem_read = 0;
            break;
        }
        case 35: { // sw
            reg_write = 0; branch = 0; jump = 0; alu_src = 1; alu_op = 0; mem_write = 1; mem_to_reg = 0; mem_read = 0;
            break;
        }
        case 51: { // R-type 
            reg_write = 1; branch = 0; jump = 0; alu_src = 0; alu_op = 2; mem_write = 0; mem_to_reg = 0; mem_read = 0;
            break;
        }
        case 99: { // branch
            reg_write = 0; branch = 1; jump = 0; alu_src = 0; alu_op = 1; mem_write = 0; mem_to_reg = 0; mem_read = 0;
            break;
        }
        case 111: { // jal
            reg_write = 1; branch = 0; jump = 1; alu_src = 0; alu_op = 0; mem_write = 0; mem_to_reg = 0; mem_read = 0;
            break;
        }
        case 103: { // jalr
            reg_write = 1; branch = 0; jump = 1; alu_src = 1; alu_op = 2; mem_write = 0; mem_to_reg = 0; mem_read = 0;
            break;
        }
        default: {
            reg_write = 0; branch = 0; jump = 0; alu_src = 0; alu_op = 0; mem_write = 0; mem_to_reg = 0; mem_read = 0;
            break;
        }
    }

    switch(alu_op) {
        case 0: alu_ctrl = 2; break; // ADD (Loads / Stores / Jumps)
        case 1: alu_ctrl = 6; break; // SUB (Branches)
        case 2: {
            if (opcode == 19) {
                if (funct3 == 0) alu_ctrl = 2; // addi
                if (funct3 == 7) alu_ctrl = 0; // andi
                if (funct3 == 6) alu_ctrl = 1; // ori
            } 
            else if (opcode == 51) {
                if (funct3 == 0 && funct7 == 0)  alu_ctrl = 2; // add
                if (funct3 == 0 && funct7 == 32) alu_ctrl = 6; // sub
                if (funct3 == 7 && funct7 == 0)  alu_ctrl = 0; // and
                if (funct3 == 6 && funct7 == 0)  alu_ctrl = 1; // or
            }
            break;
        }
    }
}

void Execute(int alu_ctrl) {
    int operandA = rs1_val;
    int operandB = (alu_src == 1) ? imm : rs2_val;

    switch (alu_ctrl) {
        case 0:  alu_result = operandA & operandB;  break;  // AND
        case 1:  alu_result = operandA | operandB;  break;  // OR
        case 2:  alu_result = operandA + operandB;  break;  // ADD
        case 6:  alu_result = operandA - operandB;  break;  // SUB
        case 7:  alu_result = (operandA < operandB) ? 1 : 0; break; // SLT
        default: alu_result = 0; break;
    }

    alu_zero = (alu_result == 0) ? 1 : 0;

    // Evaluate branch conditions to establish control behavior
    int take_branch = 0;
    if (branch) {
        if (strcmp(name, "beq") == 0 && alu_zero) take_branch = 1;
        if (strcmp(name, "bne") == 0 && !alu_zero) take_branch = 1;
        // add blt/bge here if needed
    }

    if (jump || take_branch) {
        use_branch_target = 1;
        if (jump && opcode == 103) {
            branch_target = (rs1_val + imm) & ~1;  // jalr
        } else {
            branch_target = pc + imm;                  // jal and branches
        }
    } else {
        use_branch_target = 0;
    }
}

void Mem( int addr, int write_data, int MemRead, int MemWrite ) {
    int idx = addr / 4; 
    if (MemWrite == 1) d_mem[idx] = write_data;
    if (MemRead == 1)  mem_read_data = d_mem[idx];
}

void Writeback() {
    if (reg_write == 1 && rd != 0) {
        int write_data;
        if (mem_to_reg == 1) write_data = mem_read_data;  
        else if (jump)     write_data = next_pc; // PC + 4
        else               write_data = alu_result;     

        rf[rd] = write_data;
    }
    clock_cycle++;
}
