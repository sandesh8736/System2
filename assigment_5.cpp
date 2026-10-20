/*
  FIRST PASS ONLY - a small learning version of the two-pass assembler.

  Build: g++ -std=c++11 -Wall -Wextra assembler_pass1.cpp -o pass1
  Run:   ./pass1 Instruction_Byte_Diagram.txt 2_1.asm

  Output: source.asm.pass1.txt
  No machine code is generated. No external assembler is called.

  Steps: remove comments, read labels/mnemonics/operands, choose an opcode
  table row, save its ASCII characters, calculate size, remember labels.

  Supported: .text/.data, simple labels, GLOBAL/EXTERN, BITS 32,
  DB ASCII strings, DB/DW/DD decimal integers, table forms with 0-2 operands,
  registers, decimal immediates, symbol immediates, and [register] memory.
  Not yet supported: macros, EQU, BSS, expressions, repeat prefixes,
  indexed/displaced memory, or automatic long-jump selection.
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

const int MAX_OPS = 400, MAX_SYMBOLS = 1000, MAX_LINES = 2000;
struct Opcode {
    string name, text, form, field, extra;
};
struct Symbol {
    string name, section;
    int address;
};
struct Operand {
    int kind, width, reg; // 0=none, 1=register, 2=number, 3=label, 4=memory
    long long value;
    string text;
};
struct Record {
    string source, section;
    int line, address, size, opcode, ascii[8], asciiCount;
};
Opcode table[MAX_OPS];
Symbol symbols[MAX_SYMBOLS];
Record records[MAX_LINES];
int opCount = 0, symbolCount = 0, recordCount = 0;
string error;

bool fail(string message) { error = message; return false; }
string trim(string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
string upper(string s) {
    for (unsigned int i = 0; i < s.size(); i++)
        if (s[i] >= 'a' && s[i] <= 'z') s[i] -= 'a' - 'A';
    return s;
}
// Semicolons and commas inside strings must not be treated as separators.
size_t outside(string s, char target) {
    char quote = 0;
    for (unsigned int i = 0; i < s.size(); i++) {
        if (quote) { if (s[i] == quote) quote = 0; }
        else if (s[i] == '\'' || s[i] == '"') quote = s[i];
        else if (s[i] == target) return i;
    }
    return string::npos;
}
string word(string& s) {
    s = trim(s);
    size_t end = s.find_first_of(" \t");
    string result = s.substr(0, end);
    s = end == string::npos ? "" : trim(s.substr(end));
    return result;
}
bool nameValid(string s) {
    if (s.empty()) return false;
    for (unsigned int i = 0; i < s.size(); i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
              (i > 0 && c >= '0' && c <= '9'))) return false;
    }
    return true;
}
bool number(string s, long long& value) {
    s = trim(s);
    if (s.empty()) return false;
    int sign = 1;
    if (s[0] == '+' || s[0] == '-') {
        if (s[0] == '-') sign = -1;
        s.erase(0, 1);
    }
    if (s.empty()) return false;
    value = 0;
    for (unsigned int i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        if (value > (4294967295LL - (s[i] - '0')) / 10) return false;
        value = value * 10 + s[i] - '0';
    }
    value *= sign;
    return value >= -2147483648LL;
}
bool addSymbol(string name, string section, int address) {
    if (!nameValid(name)) return fail("Invalid label: " + name);
    for (int i = 0; i < symbolCount; i++)
        if (symbols[i].name == name) return fail("Duplicate label: " + name);
    if (symbolCount == MAX_SYMBOLS) return fail("Too many labels");
    symbols[symbolCount++] = {name, section, address};
    return true;
}
bool loadTable(string filename) {
    ifstream input(filename.c_str());
    if (!input) return fail("Cannot open table: " + filename);
    string line;
    bool inTable = false;
    while (getline(input, line)) {
        line = trim(line);
        if (line == "OPCODE TABLE") { inTable = true; continue; }
        if (!inTable) continue;
        if (line == "Notes:") break;
        if (line.empty() || line[0] != '|') continue;
        string fields[8];
        size_t start = 1;
        for (int i = 0; i < 8; i++) {
            size_t end = line.find('|', start);
            if (end == string::npos) return fail("Invalid table row");
            fields[i] = trim(line.substr(start, end - start));
            start = end + 1;
        }
        if (upper(fields[0]) == "INSTRUCTION") continue;
        if (opCount == MAX_OPS) return fail("Too many table rows");
        string digits = upper(fields[1].substr(0, fields[1].find('+')));
        if (digits.empty() || digits.size() > 8 || digits.size() % 2)
            return fail("Invalid opcode length");
        for (unsigned int i = 0; i < digits.size(); i++)
            if (string("0123456789ABCDEF").find(digits[i]) == string::npos)
                return fail("Invalid opcode digit");
        table[opCount++] = {upper(fields[0]), upper(fields[1]), fields[2], fields[6], fields[7]};
    }
    if (input.bad() || opCount == 0) return fail("Cannot read opcode rows");
    return true;
}
int registerCode(string text, int& width) {
    string r32[8] = {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
    string r8[8] = {"AL","CL","DL","BL","AH","CH","DH","BH"};
    for (int i = 0; i < 8; i++) {
        if (upper(text) == r32[i]) { width = 32; return i; }
        if (upper(text) == r8[i]) { width = 8; return i; }
    }
    return -1;
}
bool parseOperand(string text, Operand& o) {
    o = Operand(); o.text = trim(text); o.reg = -1;
    if (o.text.empty()) return true;
    size_t bracket = o.text.find('[');
    if (bracket != string::npos) {
        if (o.text.back() != ']') return fail("Missing closing ]");
        string prefix = upper(trim(o.text.substr(0, bracket)));
        if (prefix == "BYTE") o.width = 8;
        else if (prefix == "DWORD") o.width = 32;
        else if (!prefix.empty()) return fail("Use BYTE or DWORD memory size");
        int baseWidth = 0;
        o.reg = registerCode(trim(o.text.substr(bracket + 1, o.text.size() - bracket - 2)), baseWidth);
        if (o.reg < 0 || baseWidth != 32) return fail("This first version supports only [32-bit register] memory");
        o.kind = 4;
    } else if ((o.reg = registerCode(o.text, o.width)) >= 0) o.kind = 1;
    else if (number(o.text, o.value)) o.kind = 2;
    else if (nameValid(o.text)) o.kind = 3;
    else return fail("Unsupported operand: " + o.text);
    return true;
}
int extraBytes(string text) {
    if (text == "imm8" || text == "rel8") return 1;
    if (text == "imm16") return 2;
    if (text == "imm32" || text == "rel32") return 4;
    return 0;
}
bool matches(string form, Operand o, Opcode op) {
    if (form.empty() || form == "-") return o.kind == 0;
    if (form == "EAX" || form == "AL" || form == "CL") return o.kind == 1 && upper(o.text) == form;
    if (form == "1") return o.kind == 2 && o.value == 1;
    if (form == "m") return o.kind == 4;
    if (form == "rel8" || form == "rel32") return o.kind == 3;
    if (form.substr(0, 3) == "imm") {
        int bytes = extraBytes(form);
        if (o.kind == 3) return bytes == 4;
        if (o.kind != 2 || bytes == 0) return false;
        long long limit = 1;
        for (int i = 0; i < bytes; i++) limit *= 256;
        bool signedOnly = op.text == "83" || (op.name == "PUSH" && bytes == 1);
        return o.value >= -limit / 2 && o.value <= (signedOnly ? limit / 2 - 1 : limit - 1);
    }
    int width = (form == "r8" || form == "r/m8") ? 8 :
                (form == "r32" || form == "r/m32") ? 32 : 0;
    if (!width || o.width != width) return false;
    return o.kind == 1 || (o.kind == 4 && form.substr(0, 3) == "r/m");
}
bool selectOpcode(string name, string text, Record& record) {
    size_t comma = outside(text, ',');
    string a = text.substr(0, comma), b = "";
    if (comma != string::npos) {
        b = trim(text.substr(comma + 1));
        if (trim(a).empty() || b.empty() || outside(b, ',') != string::npos)
            return fail("Expected one or two nonempty operands");
    }
    Operand operands[2];
    if (!parseOperand(a, operands[0]) || !parseOperand(b, operands[1])) return false;
    for (int i = 0; i < 2; i++) {
        if (operands[i].kind == 4 && operands[i].width == 0) {
            if (name == "LEA") continue;
            if (operands[1-i].kind != 1 || name == "MOVZX" || name == "MOVSX" ||
                name == "SHL" || name == "SHR" || name == "SAL" || name == "SAR" || name == "ROL" || name == "ROR")
                return fail("Specify BYTE or DWORD before memory");
            operands[i].width = operands[1-i].width;
        }
    }
    int bestSize = 100;
    for (int i = 0; i < opCount; i++) {
        Opcode op = table[i];
        if (op.name != name) continue;
        size_t separator = op.form.find(',');
        string first = op.form.substr(0, separator);
        string second = separator == string::npos ? "" : op.form.substr(separator + 1);
        if (!matches(first, operands[0], op) || !matches(second, operands[1], op)) continue;
        int size = op.text.substr(0, op.text.find('+')).size() / 2 + extraBytes(op.extra);
        if (op.field != "-") {
            size++; // ModR/M byte.
            for (int j = 0; j < 2; j++) {
                if (operands[j].kind == 4 && operands[j].reg == 4) size++; // ESP: SIB.
                if (operands[j].kind == 4 && operands[j].reg == 5) size++; // EBP: zero displacement.
            }
        }
        if (size < bestSize) { record.opcode = i; bestSize = size; }
    }
    if (record.opcode < 0) return fail("No supported opcode form for " + name);
    record.size = bestSize;
    string digits = table[record.opcode].text.substr(0, table[record.opcode].text.find('+'));
    record.asciiCount = digits.size();
    for (int i = 0; i < record.asciiCount; i++) record.ascii[i] = (unsigned char)digits[i];
    return true;
}
bool dataSize(string directive, string text, int& size) {
    int unit = directive == "DB" ? 1 : directive == "DW" ? 2 : 4;
    if (text.empty()) return fail("Missing data values");
    while (!text.empty()) {
        size_t comma = outside(text, ',');
        string item = trim(text.substr(0, comma));
        if (item.empty()) return fail("Empty data value");
        if (item[0] == '\'' || item[0] == '"') {
            if (unit != 1 || item.size() < 2 || item.back() != item[0]) return fail("Use a closed DB string");
            for (unsigned int i = 1; i + 1 < item.size(); i++)
                if ((unsigned char)item[i] > 127 || item[i] == item[0]) return fail("Use plain ASCII strings");
            if (item.size() > 1000000) return fail("Data line too large");
            size += item.size() - 2;
        } else {
            long long value = 0, limit = 1;
            for (int i = 0; i < unit; i++) limit *= 256;
            if (!number(item, value) || value < -limit/2 || value >= limit) return fail("Invalid or oversized data value");
            size += unit;
        }
        if (comma == string::npos) break;
        text = trim(text.substr(comma + 1));
        if (text.empty()) return fail("Missing data after comma");
    }
    return true;
}

bool pass1(string filename) {
    ifstream input(filename.c_str());
    if (!input) return fail("Cannot open source: " + filename);
    int address[2] = {0,0}, section = 0;
    string original;
    while (getline(input, original)) {
        if (recordCount == MAX_LINES) return fail("Too many source lines");
        Record& r = records[recordCount++];
        r.line = recordCount; r.source = original; r.opcode = -1;
        r.section = section == 0 ? ".text" : ".data"; r.address = address[section];
        string text = trim(original.substr(0, outside(original, ';')));
        if (text.empty()) continue;
        size_t colon = outside(text, ':');
        if (colon != string::npos) {
            if (!addSymbol(trim(text.substr(0, colon)), r.section, r.address)) return false;
            text = trim(text.substr(colon + 1));
            if (text.empty()) continue;
        }
        string first = word(text), mnemonic = upper(first);
        if (mnemonic == "SECTION" || mnemonic == "SEGMENT") {
            if (upper(text) == ".TEXT") section = 0;
            else if (upper(text) == ".DATA") section = 1;
            else return fail("This first version supports .text and .data only");
            continue;
        }
        if (mnemonic == "BITS") {
            if (text != "32") return fail("Only BITS 32 is supported");
            continue;
        }
        if (mnemonic == "GLOBAL" || mnemonic == "EXTERN") {
            if (!nameValid(text)) return fail("Use one simple name per declaration");
            if (mnemonic == "EXTERN" && !addSymbol(text, "extern", 0)) return false;
            continue;
        }
        string rest = text, second = upper(word(rest));
        if (second == "DB" || second == "DW" || second == "DD") {
            if (!addSymbol(first, r.section, r.address)) return false;
            mnemonic = second; text = rest;
        }
        if (mnemonic == "DB" || mnemonic == "DW" || mnemonic == "DD") {
            if (!dataSize(mnemonic, text, r.size)) return false;
        } else {
            if (section != 0) return fail("Expected a data declaration in .data");
            if (!selectOpcode(mnemonic, text, r)) return false;
        }
        if (r.size > 2147483647 - address[section]) return fail("Section is too large");
        address[section] += r.size;
    }
    return input.bad() ? fail("Error reading source") : true;
}
void showPass1(ostream& output) {
    output << "PASS 1 ONLY - all addresses and ASCII values below are decimal\n\n";
    for (int i = 0; i < recordCount; i++) {
        Record r = records[i];
        if (r.size == 0) continue;
        output << "Line " << r.line << " | " << r.section << " | address " << r.address
               << " | size " << r.size << " | " << trim(r.source) << '\n';
        if (r.opcode >= 0) {
            output << "  Table opcode: " << table[r.opcode].text << " | ASCII:";
            for (int j = 0; j < r.asciiCount; j++) output << ' ' << r.ascii[j];
            output << '\n';
        }
    }
    output << "\nSYMBOL TABLE\nName | Section | Address (decimal)\n";
    for (int i = 0; i < symbolCount; i++)
        output << symbols[i].name << " | " << symbols[i].section << " | " << symbols[i].address << '\n';
    output << "\nPass 2 is not implemented. Symbol resolution, branch-range checks,\n"
              "operand-byte generation, and final hexadecimal output come next.\n";
}
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./pass1 Instruction_Byte_Diagram.txt source.asm\n";
        return 1;
    }
    if (!loadTable(argv[1]) || !pass1(argv[2])) {
        cerr << "Error";
        if (recordCount) cerr << " on source line " << recordCount;
        cerr << ": " << error << '\n';
        return 1;
    }
    string report = string(argv[2]) + ".pass1.txt";
    if (report == argv[1]) { cerr << "Output conflicts with table filename\n"; return 1; }
    ofstream output(report.c_str());
    if (!output) { cerr << "Cannot create report\n"; return 1; }
    showPass1(output); output.close();
    if (!output) { cerr << "Could not save report\n"; return 1; }
    showPass1(cout);
    return cout ? 0 : 1;
}
