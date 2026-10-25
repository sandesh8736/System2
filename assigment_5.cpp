/*
  TWO-PASS ASSEMBLER 32-bit execution mode.
   Run:   ./assembler pass1_opcodes.txt sample.asm

  Pass 1: choose opcode forms, store opcode text as ASCII values, calculate
          sizes, save operands and data, and build the symbol table.
  Pass 2: decode saved ASCII digits, calculate operand bytes and jumps,
          and manually format the resulting bytes as hexadecimal.

  No external assembler is called. This produces listings, not object files.
  Supports the supplied table, .text/.data, simple labels, DB/DW/DD,
  8/16/32-bit data registers, and simple 32-bit memory addressing.
  Macros, EQU, BSS, indexed memory, and general expressions are not included.
*/
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

const int MAX_OPS = 400, MAX_SYMBOLS = 1000, MAX_LINES = 2000;
const int MAX_BYTES = 1024;

struct Opcode {
    string name, text, form, field, extra;
};
struct Symbol {
    string name, section;
    int address;
};
struct Operand {
    int kind;              // 0=absent, 1=register, 2=number, 3=symbol, 4=memory
    int width, reg;        // For memory, reg is the base; -1 means absolute.
    int distance;          // 0=default branch, 1=short, 4=near
    long long value;       // Immediate or memory displacement
    string text, symbol;
};
struct Record {
    string source, section;
    int address, size, opcode, ascii[8], asciiCount;
    Operand operands[2];   // Saved by pass 1, used by pass 2.
    int bytes[MAX_BYTES];
    char opening[MAX_BYTES], closing[MAX_BYTES]; // Listing annotations
    bool data;
};
Opcode table[MAX_OPS];
Symbol symbols[MAX_SYMBOLS];
Record records[MAX_LINES];
int opcodeCount = 0, symbolCount = 0, recordCount = 0, currentLine = 0;
string error;

bool fail(string message) { error = message; return false; }
string trim(string text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    return text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}
string upper(string text) {
    for (unsigned int i = 0; i < text.size(); i++)
        if (text[i] >= 'a' && text[i] <= 'z') text[i] -= 'a' - 'A';
    return text;
}
string takeWord(string& text) {
    text = trim(text);
    size_t end = text.find_first_of(" \t");
    string result = text.substr(0, end);
    text = end == string::npos ? "" : trim(text.substr(end));
    return result;
}
size_t outsideQuotes(string text, char target) {
    char quote = 0;
    for (unsigned int i = 0; i < text.size(); i++) {
        if (quote) { if (text[i] == quote) quote = 0; }
        else if (text[i] == '\'' || text[i] == '"') quote = text[i];
        else if (text[i] == target) return i;
    }
    return string::npos;
}
bool validName(string text) {
    if (text.empty()) return false;
    for (unsigned int i = 0; i < text.size(); i++) {
        char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
              (i > 0 && c >= '0' && c <= '9'))) return false;
    }
    return true;
}
int digitFromAscii(int ascii) {
    if (ascii >= 48 && ascii <= 57) return ascii - 48;
    if (ascii >= 65 && ascii <= 70) return ascii - 65 + 10;
    if (ascii >= 97 && ascii <= 102) return ascii - 97 + 10;
    return -1;
}
bool readNumber(string text, long long& value) {
    text = trim(text);
    if (text.empty()) return false;
    if (text.size() == 3 && (text[0] == '\'' || text[0] == '"') && text[2] == text[0]) {
        value = (unsigned char)text[1];
        return value <= 127;
    }
    int sign = 1, base = 10;
    if (text[0] == '+' || text[0] == '-') {
        if (text[0] == '-') sign = -1;
        text.erase(0, 1);
    }
    if (text.size() > 2 && upper(text.substr(0, 2)) == "0X") {
        base = 16; text.erase(0, 2);
    } else if (text.size() > 1 && upper(text.substr(text.size() - 1)) == "H") {
        base = 16; text.erase(text.size() - 1);
    }
    if (text.empty()) return false;
    value = 0;
    for (unsigned int i = 0; i < text.size(); i++) {
        int digit = digitFromAscii((unsigned char)text[i]);
        if (digit < 0 || digit >= base || value > (4294967295LL - digit) / base) return false;
        value = value * base + digit;
    }
    value *= sign;
    return value >= -2147483648LL;
}
int findSymbol(string name) {
    for (int i = 0; i < symbolCount; i++)
        if (symbols[i].name == name) return i;
    return -1;
}
bool addSymbol(string name, string section, int address) {
    if (!validName(name)) return fail("Invalid symbol: " + name);
    if (findSymbol(name) >= 0) return fail("Duplicate symbol: " + name);
    if (symbolCount == MAX_SYMBOLS) return fail("Too many symbols");
    symbols[symbolCount++] = {name, section, address};
    return true;
}
int extraSize(string extra) {
    if (extra == "IMM8" || extra == "REL8") return 1;
    if (extra == "IMM16") return 2;
    if (extra == "IMM32" || extra == "REL32") return 4;
    return 0;
}
bool loadTable(string filename) {
    ifstream file(filename.c_str());
    if (!file) return fail("Cannot open opcode table: " + filename);
    string line;
    bool inside = false;
    while (getline(file, line)) {
        line = trim(line);
        if (line == "OPCODE TABLE") { inside = true; continue; }
        if (!inside) continue;
        if (line == "Notes:") break;
        if (line.empty() || line[0] != '|') continue;
        string fields[8];
        size_t start = 1;
        for (int i = 0; i < 8; i++) {
            size_t end = line.find('|', start);
            if (end == string::npos) return fail("Invalid opcode table row");
            fields[i] = upper(trim(line.substr(start, end - start)));
            start = end + 1;
        }
        if (fields[0] == "INSTRUCTION") continue;
        if (opcodeCount == MAX_OPS) return fail("Too many opcode rows");
        string digits = fields[1].substr(0, fields[1].find('+'));
        if (digits.empty() || digits.size() > 8 || digits.size() % 2) return fail("Invalid opcode length");
        for (unsigned int i = 0; i < digits.size(); i++)
            if (digitFromAscii((unsigned char)digits[i]) < 0) return fail("Invalid opcode digit");
        string field = fields[6];
        if (field != "-" && field != "/R" &&
            !(field.size() == 2 && field[0] == '/' && field[1] >= '0' && field[1] <= '7'))
            return fail("Invalid ModR/M field");
        if (fields[7] != "-" && extraSize(fields[7]) == 0) return fail("Unsupported immediate/offset field");
        table[opcodeCount++] = {fields[0],fields[1],fields[2],field,fields[7]};
    }
    if (file.bad() || opcodeCount == 0) return fail("No readable opcode table found");
    return true;
}
int registerCode(string text, int& width) {
    string r32[8] = {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
    string r16[8] = {"AX","CX","DX","BX","SP","BP","SI","DI"};
    string r8[8] = {"AL","CL","DL","BL","AH","CH","DH","BH"};
    for (int i = 0; i < 8; i++) {
        if (upper(text) == r32[i]) { width = 32; return i; }
        if (upper(text) == r16[i]) { width = 16; return i; }
        if (upper(text) == r8[i]) { width = 8; return i; }
    }
    return -1;
}
bool parseOperand(string text, Operand& operand) {
    operand = Operand(); operand.reg = -1; operand.text = trim(text);
    text = operand.text;
    if (text.empty()) return true;
    string copy = text, first = upper(takeWord(copy));
    if (first == "NEAR" || first == "SHORT") {
        operand.distance = first == "NEAR" ? 4 : 1;
        text = copy;
        if (!validName(text)) return fail("NEAR/SHORT requires a label");
    }
    size_t bracket = text.find('[');
    if (bracket != string::npos) {
        if (text.back() != ']') return fail("Missing closing ]");
        string prefix = upper(trim(text.substr(0, bracket)));
        if (prefix == "BYTE") operand.width = 8;
        else if (prefix == "WORD") operand.width = 16;
        else if (prefix == "DWORD") operand.width = 32;
        else if (!prefix.empty()) return fail("Use BYTE, WORD, or DWORD before memory");
        string address = trim(text.substr(bracket + 1, text.size() - bracket - 2));
        size_t sign = address.find_first_of("+-", 1);
        int width = 0;
        operand.reg = registerCode(trim(address.substr(0, sign)), width);
        if (operand.reg >= 0) {
            if (width != 32) return fail("Memory addresses need a 32-bit register, e.g. [EBX], not [BX]");
            if (sign != string::npos) {
                string displacement;
                for (size_t i = sign; i < address.size(); i++)
                    if (address[i] != ' ' && address[i] != '\t') displacement += address[i];
                if (!readNumber(displacement, operand.value) || operand.value > 2147483647LL)
                    return fail("Use a signed numeric displacement such as [EBX-4]");
            }
        } else if (readNumber(address, operand.value)) { }
        else if (validName(address)) operand.symbol = address;
        else return fail("Unsupported memory address: " + address);
        operand.kind = 4;
    } else if ((operand.reg = registerCode(text, operand.width)) >= 0) operand.kind = 1;
    else if (readNumber(text, operand.value)) operand.kind = 2;
    else if (validName(text)) { operand.kind = 3; operand.symbol = text; }
    else return fail("Invalid operand: " + text);
    return true;
}
bool fits(long long value, int count, bool signedOnly) {
    long long limit = 1;
    for (int i = 0; i < count; i++) limit *= 256;
    return value >= -limit / 2 && value <= (signedOnly ? limit / 2 - 1 : limit - 1);
}
int displacementSize(Operand memory) {
    if (memory.reg < 0) return 4;
    if (memory.value == 0 && memory.reg != 5) return 0;
    if (memory.value >= -128 && memory.value <= 127) return 1;
    return 4;
}
void formParts(string form, string& first, string& second) {
    size_t comma = form.find(',');
    first = form.substr(0, comma);
    second = comma == string::npos ? "" : form.substr(comma + 1);
}
bool matches(string form, Operand operand, Opcode op) {
    if (form.empty() || form == "-") return operand.kind == 0;
    if (form == "REL8" || form == "REL32")
        return operand.kind == 3 && (operand.distance == 0 || operand.distance == extraSize(form));
    if (operand.distance != 0) return false;
    if (form == "EAX" || form == "AX" || form == "AL" || form == "CL")
        return operand.kind == 1 && upper(operand.text) == form;
    if (form == "1") return operand.kind == 2 && operand.value == 1;
    if (form == "M") return operand.kind == 4;
    if (form.substr(0,3) == "IMM") {
        int size = extraSize(form);
        if (operand.kind == 3) return size == 4; // 32-bit symbol-address fields only.
        bool signedOnly = op.text == "83" || op.text == "6683" || (op.name == "PUSH" && size == 1);
        return operand.kind == 2 && size > 0 && fits(operand.value, size, signedOnly);
    }
    int width = (form == "R8" || form == "R/M8") ? 8 :
                (form == "R16" || form == "R/M16") ? 16 :
                (form == "R32" || form == "R/M32") ? 32 : 0;
    return width && operand.width == width &&
        (operand.kind == 1 || (operand.kind == 4 && form.substr(0,3) == "R/M"));
}
int rmOperand(Opcode op) {
    string first, second; formParts(op.form, first, second);
    return first.substr(0,3) == "R/M" || first == "M" ? 0 : 1;
}
bool selectOpcode(string mnemonic, string text, Record& record) {
    size_t comma = outsideQuotes(text, ',');
    string a = text.substr(0, comma), b = "";
    if (comma != string::npos) {
        b = trim(text.substr(comma + 1));
        if (trim(a).empty() || b.empty() || outsideQuotes(b, ',') != string::npos)
            return fail("Expected at most two nonempty operands");
    }
    if (!parseOperand(a, record.operands[0]) || !parseOperand(b, record.operands[1])) return false;
    for (int i = 0; i < 2; i++) {
        Operand& memory = record.operands[i];
        if (memory.kind == 4 && memory.width == 0) {
            if (mnemonic == "LEA") continue;
            string explicitSize = " MOVZX MOVSX SHL SHR SAL SAR ROL ROR ";
            if (record.operands[1-i].kind != 1 || explicitSize.find(" " + mnemonic + " ") != string::npos)
                return fail("Specify BYTE, WORD, or DWORD before memory");
            memory.width = record.operands[1-i].width;
        }
    }
    int bestSize = 100;
    bool known = false;
    for (int i = 0; i < opcodeCount; i++) {
        Opcode op = table[i];
        if (op.name != mnemonic) continue;
        known = true;
        string first, second; formParts(op.form, first, second);
        if (!matches(first, record.operands[0], op) || !matches(second, record.operands[1], op)) continue;
        int size = op.text.substr(0, op.text.find('+')).size() / 2 + extraSize(op.extra);
        if (op.field != "-") {
            size++; // ModR/M
            Operand memory = record.operands[rmOperand(op)];
            if (memory.kind == 4) {
                size += displacementSize(memory);
                if (memory.reg == 4) size++; // SIB for ESP
            }
        }
        if (size < bestSize) { record.opcode = i; bestSize = size; }
    }
    if (!known) return fail("Mnemonic missing from opcode table: " + mnemonic);
    if (record.opcode < 0) return fail("No matching table form for " + mnemonic + " " + text);
    record.size = bestSize;
    string digits = table[record.opcode].text.substr(0, table[record.opcode].text.find('+'));
    record.asciiCount = digits.size();
    for (int i = 0; i < record.asciiCount; i++) record.ascii[i] = (unsigned char)digits[i];
    return true;
}
bool putValue(Record& record, int& position, long long value, int count) {
    if (!fits(value, count, false) || position + count > MAX_BYTES) return fail("Byte value or buffer size is out of range");
    if (value < 0) {
        long long limit = 1;
        for (int i = 0; i < count; i++) limit *= 256;
        value += limit;
    }
    for (int i = 0; i < count; i++) {
        record.bytes[position++] = value % 256;
        value /= 256;
    }
    return true;
}
bool readData(string directive, string text, Record& record) {
    int unit = directive == "DB" ? 1 : directive == "DW" ? 2 : 4;
    record.data = true;
    if (text.empty()) return fail("Missing data value");
    while (!text.empty()) {
        size_t comma = outsideQuotes(text, ',');
        string item = trim(text.substr(0, comma));
        if (item.empty()) return fail("Empty data value");
        if (item[0] == '\'' || item[0] == '"') {
            if (unit != 1 || item.size() < 2 || item.back() != item[0]) return fail("Use a closed DB string");
            for (unsigned int i = 1; i + 1 < item.size(); i++) {
                int ascii = (unsigned char)item[i];
                if (ascii > 127 || item[i] == item[0]) return fail("Use plain ASCII strings without escapes");
                if (!putValue(record, record.size, ascii, 1)) return false;
            }
        } else {
            long long number;
            if (!readNumber(item, number)) return fail("Data requires a number or DB string");
            if (!putValue(record, record.size, number, unit)) return false;
        }
        if (comma == string::npos) break;
        text = trim(text.substr(comma + 1));
        if (text.empty()) return fail("Missing data after comma");
    }
    return true;
}

bool pass1(string filename) {
    ifstream input(filename.c_str());
    if (!input) return fail("Cannot open assembly source: " + filename);
    int address[2] = {0,0}, section = 0;
    string original;
    while (getline(input, original)) {
        currentLine = recordCount + 1;
        if (recordCount == MAX_LINES) return fail("Too many source lines");
        Record& record = records[recordCount++];
        record.source = original; record.opcode = -1;
        record.section = section == 0 ? ".text" : ".data";
        record.address = address[section];
        string text = trim(original.substr(0, outsideQuotes(original, ';')));
        if (text.empty()) continue;
        size_t colon = outsideQuotes(text, ':');
        if (colon != string::npos) {
            if (!addSymbol(trim(text.substr(0, colon)), record.section, record.address)) return false;
            text = trim(text.substr(colon + 1));
            if (text.empty()) continue;
        }
        string first = takeWord(text), mnemonic = upper(first);
        if (mnemonic == "SECTION" || mnemonic == "SEGMENT") {
            if (upper(text) == ".TEXT") section = 0;
            else if (upper(text) == ".DATA") section = 1;
            else return fail("This learning version supports only .text and .data");
            continue;
        }
        if (mnemonic == "BITS") {
            if (text != "32") return fail("Use BITS 32");
            continue;
        }
        if (mnemonic == "GLOBAL" || mnemonic == "EXTERN") {
            if (!validName(text)) return fail("Use one symbol per GLOBAL/EXTERN line");
            if (mnemonic == "EXTERN" && !addSymbol(text, "extern", 0)) return false;
            continue;
        }
        string rest = text, second = upper(takeWord(rest));
        if (second == "DB" || second == "DW" || second == "DD") {
            if (!addSymbol(first, record.section, record.address)) return false;
            mnemonic = second; text = rest;
        }
        if (mnemonic == "DB" || mnemonic == "DW" || mnemonic == "DD") {
            if (!readData(mnemonic, text, record)) return false;
        } else {
            if (section != 0) return fail("Instructions must be in .text");
            if (!selectOpcode(mnemonic, text, record)) return false;
        }
        if (record.size > 2147483647 - address[section]) return fail("Section is too large");
        address[section] += record.size;
    }
    return input.bad() ? fail("Error reading assembly file") : true;
}

bool putSymbol(Record& record, int& position, string name, int count, bool relative) {
    int index = findSymbol(name);
    if (index < 0) return fail("Undefined symbol: " + name);
    Symbol symbol = symbols[index];
    long long value = symbol.address;
    char opening = '[', closing = ']';
    if (relative) {
        if (symbol.section == record.section) {
            value -= record.address + record.size;
            opening = 0; closing = 0;
        } else if (symbol.section == "extern") {
            if (count != 4) return fail("Use NEAR for an external jump");
            value = 0; opening = '('; closing = ')';
        } else return fail("Relative branches between different sections are not supported");
        if (!fits(value, count, true)) return fail("Jump out of range: use NEAR if the table has a rel32 form");
    }
    record.opening[position] = opening;
    record.closing[position + count - 1] = closing;
    return putValue(record, position, value, count);
}
bool putModRM(Record& record, Opcode op, int& position) {
    int index = rmOperand(op);
    Operand memory = record.operands[index];
    int reg = op.field == "/R" ? record.operands[1-index].reg : op.field[1] - '0';
    if (reg < 0 || reg > 7) return fail("Invalid register field");
    if (memory.kind == 1)
        return putValue(record, position, 3*64 + reg*8 + memory.reg, 1);
    if (memory.kind != 4) return fail("Expected a register or memory operand");
    int count = displacementSize(memory), mod = 0;
    if (memory.reg >= 0 && count == 1) mod = 1;
    if (memory.reg >= 0 && count == 4) mod = 2;
    int rm = memory.reg < 0 ? 5 : memory.reg;
    if (!putValue(record, position, mod*64 + reg*8 + rm, 1)) return false;
    if (memory.reg == 4 && !putValue(record, position, 36, 1)) return false;
    if (count == 0) return true;
    if (!memory.symbol.empty()) return putSymbol(record, position, memory.symbol, count, false);
    return putValue(record, position, memory.value, count);
}
bool pass2() {
    for (int i = 0; i < recordCount; i++) {
        currentLine = i + 1;
        Record& record = records[i];
        if (record.opcode < 0) continue; // Data already contains decimal bytes.
        Opcode op = table[record.opcode];
        int position = 0;
        for (int j = 0; j < record.asciiCount; j += 2) {
            int high = digitFromAscii(record.ascii[j]);
            int low = digitFromAscii(record.ascii[j+1]);
            if (high < 0 || low < 0) return fail("Invalid saved opcode ASCII value");
            int byte = high*16 + low;
            if (j == record.asciiCount - 2 && op.text.find('+') != string::npos)
                byte += record.operands[0].reg;
            if (!putValue(record, position, byte, 1)) return false;
        }
        if (op.field != "-" && !putModRM(record, op, position)) return false;
        int count = extraSize(op.extra);
        if (count > 0) {
            Operand operand = record.operands[record.operands[1].kind == 0 ? 0 : 1];
            if (operand.kind == 3) {
                if (!putSymbol(record, position, operand.symbol, count, op.extra.substr(0,3) == "REL")) return false;
            } else if (!putValue(record, position, operand.value, count)) return false;
        }
        if (position != record.size) return fail("Instruction size differs between the two passes");
    }
    return true;
}
string toHex(unsigned long long value, int width) {
    string digits = "0123456789ABCDEF", result;
    do {
        result = digits[value % 16] + result;
        value /= 16;
    } while (value != 0);
    while ((int)result.size() < width) result = "0" + result;
    return result;
}
string pad(string text, int width) {
    while ((int)text.size() < width) text += ' ';
    return text;
}
void showPass1(ostream& output) {
    output << "PASS 1: decimal addresses, sizes, and opcode-text ASCII values\n\n";
    for (int i = 0; i < recordCount; i++) {
        Record& record = records[i];
        if (record.size == 0) continue;
        output << "Line " << i+1 << " | " << record.section << " | address " << record.address
               << " | size " << record.size;
        if (record.opcode >= 0) {
            output << " | opcode " << table[record.opcode].text << " | ASCII:";
            for (int j = 0; j < record.asciiCount; j++) output << ' ' << record.ascii[j];
        } else {
            output << " | data bytes (ASCII for strings):";
            for (int j = 0; j < record.size; j++) output << ' ' << record.bytes[j];
        }
        output << '\n';
    }
    output << "\nSYMBOL TABLE (decimal addresses)\n";
    for (int i = 0; i < symbolCount; i++)
        output << symbols[i].name << " | " << symbols[i].section << " | " << symbols[i].address << '\n';
}
void showListing(ostream& output) {
    for (int i = 0; i < recordCount; i++) {
        Record& record = records[i];
        if (record.size == 0) { output << string(24,' ') << record.source << '\n'; continue; }
        int chunk = record.data ? 9 : record.size;
        for (int start = 0; start < record.size; start += chunk) {
            int end = start + chunk;
            if (end > record.size) end = record.size;
            string hexBytes;
            for (int j = start; j < end; j++) {
                if (record.opening[j]) hexBytes += record.opening[j];
                hexBytes += toHex(record.bytes[j], 2);
                if (record.closing[j]) hexBytes += record.closing[j];
            }
            if (end < record.size) hexBytes += '-';
            output << pad(hexBytes, 24);
            if (start == 0) output << record.source;
            output << '\n';
        }
    }
}
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./assembler pass1_opcodes.txt source.asm\n";
        return 1;
    }
    if (!loadTable(argv[1]) || !pass1(argv[2]) || !pass2()) {
        cerr << "Error";
        if (currentLine) cerr << " on source line " << currentLine;
        cerr << ": " << error << '\n';
        return 1;
    }
    string reportName = string(argv[2]) + ".pass1.txt";
    string listingName = string(argv[2]) + ".lst";
    if (reportName == argv[1] || listingName == argv[1]) {
        cerr << "Output filename conflicts with opcode table\n";
        return 1;
    }
    ofstream report(reportName.c_str()), listing(listingName.c_str());
    if (!report || !listing) { cerr << "Cannot create output files\n"; return 1; }
    showPass1(report); showListing(listing);
    report.close(); listing.close();
    if (!report || !listing) { cerr << "Error saving output files\n"; return 1; }
    showListing(cout);
    return cout ? 0 : 1;
}
