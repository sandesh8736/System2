#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>

using namespace std;
const int MAX_OPCODES = 100;
 
struct OpcodeInfo
{
    string mnemonic;
    string opcode;
    string operandFormat;
};

 OpcodeInfo opcodeTable[100];
 int opcodeCount = 0;
 
string toUpper(string str)
{
    for (char &ch : str)
        ch = toupper(static_cast<unsigned char>(ch));

    return str;
}
 
string trim(string str)
{
    size_t start = str.find_first_not_of(" \t\r\n");

    if (start == string::npos)
        return "";

    size_t end = str.find_last_not_of(" \t\r\n");

    return str.substr(start, end - start + 1);
}

string removeComment(string line)
{
    size_t position = line.find(';');

    if (position != string::npos)
        line = line.substr(0, position);

    return trim(line);
}

bool isRegister(string operand)
{
    operand = toUpper(trim(operand));

    string registers[] =
    {
        "EAX", "EBX", "ECX", "EDX",
        "ESI", "EDI", "ESP", "EBP",

        "AX", "BX", "CX", "DX",
        "SI", "DI", "SP", "BP",

    };

    int registerCount = 16;

    for (int i = 0; i < registerCount; i++)
    {
        if (operand == registers[i])
        {
            return true;
        }
    }

    return false;
}

bool isConstant(string operand)
{
    operand = trim(operand);

    if (operand.length() == 0)
        return false;

    if (operand[0] == '+' || operand[0] == '-')
        operand = operand.substr(1);

    if (operand.length() == 0)
        return false;

    if (operand.length() > 2 && operand[0] == '0' &&
        (operand[1] == 'x' || operand[1] == 'X'))
    {
        for (size_t i = 2; i < operand.length(); i++)
        {
            if (!isxdigit(operand[i]))
                return false;
        }

        return true;
    }

    if (operand[operand.length() - 1] == 'H' ||
        operand[operand.length() - 1] == 'h')
    {
        if (operand.length() == 1)
            return false;


    for (int i = 0; i < operand.length() - 1; i++)
        {
            if (!isxdigit(operand[i]))
                return false;
        }

        return true;
    }
 
    for (int i = 0; i < operand.length(); i++)
    {
        if (!isdigit(operand[i]))
        {
            return false;
        }
    }


    return true;
}
 
bool isMemoryOperand(string operand)
{
    operand = trim(operand);

    if (operand.length() >= 2) 
    {
         if(operand[0] == '[' && operand[operand.length() - 1] == ']')
        {
         return true;
        }
    }
    return false;
}

bool isSymbol(string operand)
{
    operand = trim(operand);

    if (operand.length() == 0)
        return false;

    if (!isalpha(operand[0]) && operand[0] != '_')
    {
        return false;   
    }

    for (int i = 0; i < operand.length(); i++)
    {
        if (!isalnum(operand[i]) && operand[i] != '_')
        {
            return false;
        }
    }

    if (isRegister(operand))
        return false;

    return true;
}

string getOperandType(string operand)
{
    operand = trim(operand);

    if (operand.empty())
        return "None";

    if (isRegister(operand))
        return "Register";

    if (isMemoryOperand(operand))
        return "Memory";

    if (isConstant(operand))
        return "Constant";

    if (isSymbol(operand))
        return "Symbol";

    return "Unknown";
}

bool readOpcodeFile( string filename,OpcodeInfo opcodeTable[], int &opcodeCount)
{
    ifstream file(filename);

    if (!file)
    {
        cout << "Error: Cannot open opcode file.\n";
        return false;
    }

    string line;

    while (getline(file, line))
    {
        line = trim(line);

        if (line.length() == 0)
            continue;

        if (line[0] == '+' || line[0] == '=' || line[0] == '-')
            continue;

        if (line.find('|') != string::npos)
        {
            string columns[10];
            int columnCount = 0;
            
            string column;

            stringstream ss(line);

            while (getline(ss, column, '|'))
            {
                 if (columnCount < 10)
                 {
                    columns[columnCount] = trim(column);
                    columnCount++;
                 }
            }

            if (columnCount >= 4)
            {
                string mnemonic = toUpper(columns[1]);
                string opcode = columns[2];
                string operandFormat = columns[3];

                // Skip header
                if (mnemonic == "INSTRUCTION" ||
                    mnemonic == "NAME")
                {
                    continue;
                }

                if (mnemonic.length()>0 && opcode.length()>0)
                {
                    if (opcodeCount < MAX_OPCODES)
                    {
                        opcodeTable[opcodeCount].mnemonic = mnemonic;

                        opcodeTable[opcodeCount].opcode = opcode;

                        opcodeTable[opcodeCount].operandFormat = operandFormat;

                        opcodeCount++;
                    }
                }
            }
        }
   else
        {
            string mnemonic;
            string opcode;
            string operandFormat;


            stringstream ss(line);


            ss >> mnemonic;
            ss >> opcode;


            getline(ss, operandFormat);


            mnemonic = toUpper(trim(mnemonic));
            opcode = trim(opcode);
            operandFormat = trim(operandFormat);


            // Skip headings
            if (mnemonic == "INSTRUCTION" ||
                mnemonic == "NAME" ||
                mnemonic == "OPCODE")
            {
                continue;
            }


            if (mnemonic.length() > 0 &&
                opcode.length() > 0)
            {
                if (opcodeCount < MAX_OPCODES)
                {
                    opcodeTable[opcodeCount].mnemonic =
                        mnemonic;

                    opcodeTable[opcodeCount].opcode =
                        opcode;

                    opcodeTable[opcodeCount].operandFormat =
                        operandFormat;

                    opcodeCount++;
                }
            }
        }
    }


    file.close();

    return true;
}

bool searchMnemonic(
    string mnemonic,
    OpcodeInfo opcodeTable[],
    int opcodeCount)
{
    for (int i = 0; i < opcodeCount; i++)
    {
        if (opcodeTable[i].mnemonic == mnemonic)
        {
            return true;
        }
    }


    return false;
}

void displayOpcodeInformation(
    string mnemonic,
    OpcodeInfo opcodeTable[],
    int opcodeCount)
{
    bool found = false;


    for (int i = 0; i < opcodeCount; i++)
    {
        if (opcodeTable[i].mnemonic == mnemonic)
        {
            found = true;


            cout << "  Opcode         : "
                 << opcodeTable[i].opcode << endl;

            cout << "  Operand Format : "
                 << opcodeTable[i].operandFormat << endl;
        }
    }


    if (!found)
    {
        cout << "  No opcode information found." << endl;
    }
}

int splitOperands(string operandText,string operands[])
{
    int operandCount = 0;

    string current = "";

    int bracketLevel = 0;


    for (int i = 0; i < operandText.length(); i++)
    {
        char ch = operandText[i];


        if (ch == '[')
        {
            bracketLevel++;
        }


        if (ch == ']')
        {
            bracketLevel--;
        }


        // Comma outside [] means new operand
        if (ch == ',' && bracketLevel == 0)
        {
            if (trim(current).length() > 0)
            {
                operands[operandCount] = trim(current);
                operandCount++;
            }

            current = "";
        }
        else
        {
            current += ch;
        }
    }


    // Add final operand
    if (trim(current).length() > 0)
    {   
        if (operandCount < 10)
        {
        operands[operandCount] = trim(current);
        operandCount++;
        }
    }

    return operandCount;
}
 
void processAssemblyFile(
    string filename,OpcodeInfo opcodeTable[],int opcodeCount)
 {
    ifstream file(filename);

    if (!file)
    {
        cout << "Error: Cannot open assembly file.\n";
        return;
    }

    string line;
    int lineNumber = 0;

    cout << "\n";
    cout << "============================================================\n";
    cout << "                 ASSEMBLY PROGRAM ANALYSIS\n";
    cout << "============================================================\n\n";

    while (getline(file, line))
    {
        lineNumber++;

        line = removeComment(line);

        if (line.length() == 0)
            continue;

         
        int colonPosition = line.find(':');

        if (colonPosition != string::npos)
        {
            string label = trim(line.substr(0, colonPosition));

            if (label.length() > 0)
            {
                cout << "Line " << lineNumber << "\n";
                cout << "Symbol : " << label << "\n";
                cout << "Type   : Symbol\n\n";
            }

            line = trim(line.substr(colonPosition + 1));

            if (line.length() == 0)
                continue;
        }

        string mnemonic;
        string operandText;

        stringstream ss(line);

        ss >> mnemonic;

        getline(ss, operandText);

        mnemonic = toUpper(trim(mnemonic));
        operandText = trim(operandText);

        
        bool found = searchMnemonic(mnemonic,opcodeTable,opcodeCount);

        cout << "------------------------------------------------------------\n";
        cout << "Line              : " << lineNumber << "\n";
        cout << "Mnemonic          : " << mnemonic << "\n";

         if (found)
        {
            cout << "Exists in Opcode  : YES\n";


            displayOpcodeInformation(
                mnemonic,
                opcodeTable,
                opcodeCount);
        }
        else
        {
            cout << "Exists in Opcode  : NO\n";
        }

        string operands[10];

         int operandCount = splitOperands(operandText,operands);

        if (operandCount == 0)
        {
            cout << "Operand           : None\n";
            cout << "Operand Type      : None\n";
        }
        else
        {
            for (int i = 0;
                 i < operandCount;
                 i++)
            {
                cout << "Operand           : "
                     << operands[i] << endl;


                cout << "Operand Type      : "
                     << getOperandType(operands[i])
                     << endl;
            }
        }
    }

    file.close();
}

 
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "Usage: "
             << argv[0]
             << " <opcode_file> <assembly_file>"
             << endl;

        return 1;
    }

    string opcodeFilename = argv[1];
    string assemblyFilename = argv[2];

    OpcodeInfo opcodeTable[MAX_OPCODES];

    int opcodeCount = 0;

    bool success =
        readOpcodeFile(
            opcodeFilename,
            opcodeTable,
            opcodeCount);

    if (!success)
    {
        return 1;
    }

    cout << "\nOpcode file loaded successfully.\n";

    cout << "Total opcode entries: "
         << opcodeCount << endl;

    processAssemblyFile(
        assemblyFilename,
        opcodeTable,
        opcodeCount);

    return 0;
}