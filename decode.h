#include <stdio.h>
#include <string.h>

char char_instr[33];

uint32_t int_instr = ( uint32_t ) strtol( char_instr, NULL, 2 );

// Decode standard RISC-V fields using bit masks and shifts
uint32_t opcode  = int_instr & 0x7F;             // Bits 0-6
uint32_t rd      = (int_instr >> 7) & 0x1F;      // Bits 7-11
uint32_t funct3  = (int_instr >> 12) & 0x07;     // Bits 12-14
uint32_t rs1     = (int_instr >> 15) & 0x1F;     // Bits 15-19
uint32_t rs2     = (int_instr >> 20) & 0x1F;     // Bits 20-24
uint32_t funct7  = (int_instr >> 25) & 0x7F;     // Bits 25-31

void Decode() {

	// --- R-TYPE INSTRUCTIONS ---
	    if (opcode == 0x33 || opcode == 0x3B) { // 0110011 or 0111011
	        const char* op_name = "unknown";

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

	        printf("Instruction Type: R\n");
	        printf("Operation: %s\n", op_name);
	        printf("Rd: %u, Funct3: %u, Rs1: %u, Rs2: %u, Funct7: 0x%02X\n", rd, funct3, rs1, rs2, funct7);
	    }

	    // --- I-TYPE INSTRUCTIONS ---
	    else if (opcode == 0x67 || opcode == 0x03 || opcode == 0x13) { // 1100111, 0000011, 0010011
	        const char* op_name = "unknown";

	        // Extract 12-bit immediate and sign-extend to 32-bit int
	        int32_t imm_i = (int32_t)int_instr >> 20;

	        if (opcode == 0x13) { // Immediate Arithmetic
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
	        } else if (opcode == 0x03) { // Loads
	            switch (funct3) {
	                case 0x0: op_name = "lb"; break;
	                case 0x1: op_name = "lh"; break;
	                case 0x2: op_name = "lw"; break;
	            }
	        } else if (opcode == 0x67) {
	            op_name = "jalr";
	        }

	        printf("Instruction Type: I\n");
	        printf("Operation: %s\n", op_name);
	        printf("Rd: %u, Funct3: %u, Rs1: %u, Immediate: %d\n", rd, funct3, rs1, imm_i);
	    }

	    // --- S-TYPE INSTRUCTIONS (Stores) ---
	    else if (opcode == 0x23) { // 0100011
	        const char* op_name = "unknown";
	        switch (funct3) {
	            case 0x0: op_name = "sb"; break;
	            case 0x1: op_name = "sh"; break;
	            case 0x2: op_name = "sw"; break;
	        }

	        // Reconstruct S-Immediate from split fields (Bits 31:25 and 11:7)
	        int32_t imm_s = ((int32_t)(int_instr & 0xFE000000) >> 20) | ((int_instr >> 7) & 0x1F);

	        printf("Instruction Type: S\n");
	        printf("Operation: %s\n", op_name);
	        printf("Rs1: %u, Rs2: %u, Funct3: %u, Immediate: %d\n", rs1, rs2, funct3, imm_s);
	    }

	    // --- B-TYPE INSTRUCTIONS (Branches) ---
	    else if (opcode == 0x63) { // 1100011
	        const char* op_name = "unknown";
	        switch (funct3) {
	            case 0x0: op_name = "beq"; break;
	            case 0x1: op_name = "bne"; break;
	            case 0x4: op_name = "blt"; break;
	            case 0x5: op_name = "bge"; break;
	        }

	        // Reconstruct split B-Immediate & force sign extension
	        int32_t imm_b = ((int32_t)(int_instr & 0x80000000) >> 19) | // bit 12
	                        ((int_instr & 0x00000080) << 4)  | // bit 11
	                        ((int_instr & 0x7E000000) >> 20) | // bits 10:5
	                        ((int_instr & 0x00000F00) >> 7);   // bits 4:1
	        // Bit 0 is implicitly 0 in RISC-V branches

	        printf("Instruction Type: B (SB)\n");
	        printf("Operation: %s\n", op_name);
	        printf("Rs1: %u, Rs2: %u, Funct3: %u, Immediate: %d\n", rs1, rs2, funct3, imm_b);
	    }

	    // --- U-TYPE INSTRUCTIONS ---
	    else if (opcode == 0x17 || opcode == 0x37) { // 0010111 or 0110111
	        const char* op_name = (opcode == 0x17) ? "auipc" : "lui";
	        uint32_t imm_u = int_instr & 0xFFFFF000; // Upper 20 bits

	        printf("Instruction Type: U\n");
	        printf("Operation: %s\n", op_name);
	        printf("Rd: %u, Immediate (Hex): 0x%08X\n", rd, imm_u);
	    }

	    // --- J-TYPE INSTRUCTIONS (jal) ---
	    else if (opcode == 0x6F) { // 1101111
	        // Reconstruct highly scrambled J-Immediate & force sign extension
	        int32_t imm_j = ((int32_t)(int_instr & 0x80000000) >> 11) | // bit 20
	                        (int_instr & 0x000FF000)         | // bits 19:12
	                        ((int_instr & 0x00100000) >> 9)  | // bit 11
	                        ((int_instr & 0x7FE00000) >> 20);  // bits 10:1
	        // Bit 0 is implicitly 0

	        printf("Instruction Type: J (UJ)\n");
	        printf("Operation: jal\n");
	        printf("Rd: %u, Immediate: %d\n", rd, imm_j);
	    }

	    else {
	        printf("Unknown Opcode: 0x%02X\n", opcode);
	    }

	    return 0;
	}

}
