# Assigement3
This is a C++ program that reads an opcode reference file and analyzes an assembly source file to determine whether each instruction mnemonic exists in the opcode table and what type of operands it uses.
This is useful for learning and understanding instruction encoding and operand classification in x86 assembly.

## How the Program Works

### 1. Opcode File Loading
The program reads the opcode file and stores entries in an internal table. Each entry includes:
- mnemonic,
- opcode,
- operand format.

### 2. Assembly File Analysis
For every line in the assembly file, the program:
- removes comments starting with `;`,
- extracts labels before a colon,
- reads the mnemonic and operands,
- searches the opcode table for the mnemonic,
- prints its opcode information if found,
- splits multiple operands separated by commas,
- identifies the operand type.

### 3. Operand Classification
The program recognizes operands such as:
- registers: `EAX`, `ECX`, `EBX`, `AX`, `BX`, etc.
- constants: `5`, `10`, `0x10`, `0AH`
- memory operands: `[ebx + esi*4 + 0x10]`
- symbols: labels like `start`, `subroutine`

## Build and Run
Compile:
g++ assigement3.cpp

Run it with the opcode table and assembly file:
./a.out opcode.txt sample.asm

- assembly language parsing,
- opcode tables,
- operand classification,
- x86 addressing and instruction format understanding.


# Assigment4
- This program reads an assembly file and an opcode table, then generates the corresponding machine-code bytes for each instruction.

 ## How programs are work 
- Parses assembly instructions from a `.asm` file
- Removes comments starting with `;`
- Supports basic register and memory operands
- Handles simple data directives like `db`, `dw`, and `dd`
- Matches instructions against a predefined opcode table
- Encodes immediate values and addressing modes
- Prints generated hexadecimal bytes next to the original source line

## Input files
1. An assembly source file (for example `program.asm`)
2. A file containing the instruction encoding table (for example `Instruction_Byte_Diagram.txt`)

## Compile
g++ " Assigment4.cpp" 

## Run
./a.out program.asm Instruction_Byte_Diagram.txt


# Assigment5
Reads opcode/instruction definitions from a table file
- Supports `.text` and `.data` sections
- Handles labels and symbols
- Supports basic data directives such as `DB`, `DW`, and `DD`
- Matches instructions against valid opcode forms
- Calculates instruction sizes and operand values
- Produces a pass-1 report and a listing file

# Assigment6
- Converts supported x86 instructions into machine-code bytes.
- Identifies flags modified by instructions.
- Identifies flags checked by conditional jumps.
- Describes EFLAGS behavior for arithmetic, logical, shift, rotate, and control-flow instructions.
- Generates an instruction listing with addresses, hexadecimal bytes, and flag information.
- Generates instruction_flags.txt containing flag metadata and explanatory notes.
- Generates symbol_table.txt with symbol information.

## EFLAGS Analysis
The analyzer identifies important flags, including:
- CF — Carry Flag
- ZF — Zero Flag
- SF — Sign Flag
- OF — Overflow Flag
- PF — Parity Flag
- AF — Auxiliary Carry Flag
- DF — Direction Flag
- IF — Interrupt Flag
It distinguishes between flags modified and flags checked, helping explain how instructions such as CMP, JE, JNE, and JG interact.
