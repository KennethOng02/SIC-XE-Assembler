// reference
// https://github.com/cppcoders/SIC-XE-Assembler/tree/master

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
using namespace std;

struct OP {
    int format;
    string opcode;
};

struct LINE {
    int locctr;
    string label;
    string mnemonic;
    string target;
    string object_code;
};

class Assembler {
    ifstream input_file;

    map<string, OP> OPTAB = {
        {"ADD", {3, "18"}},
        {"ADDF", {3, "58"}},
        {"ADDR", {2, "90"}},
        {"AND", {3, "40"}},
        {"CLEAR", {2, "B4"}},
        {"COMP", {3, "28"}},
        {"COMPF", {3, "88"}},
        {"COMPR", {2, "A0"}},
        {"DIV", {3, "24"}},
        {"DIVF", {3, "64"}},
        {"DIVR", {2, "9C"}},
        {"FIX", {1, "C4"}},
        {"FLOAT", {1, "C0"}},
        {"HIO", {1, "F4"}},
        {"J", {3, "3C"}},
        {"JEQ", {3, "30"}},
        {"JGT", {3, "34"}},
        {"JLT", {3, "38"}},
        {"JSUB", {3, "48"}},
        {"LDA", {3, "00"}},
        {"LDB", {3, "68"}},
        {"LDCH", {3, "50"}},
        {"LDF", {3, "70"}},
        {"LDL", {3, "08"}},
        {"LDS", {3, "6C"}},
        {"LDT", {3, "74"}},
        {"LDX", {3, "04"}},
        {"LPS", {3, "D0"}},
        {"MUL", {3, "20"}},
        {"MULF", {3, "60"}},
        {"MULR", {2, "98"}},
        {"NORM", {1, "C8"}},
        {"OR", {3, "44"}},
        {"RD", {3, "D8"}},
        {"RMO", {2, "AC"}},
        {"RSUB", {3, "4C"}},
        {"SHIFTL", {2, "A4"}},
        {"SHIFTR", {2, "A8"}},
        {"SIO", {1, "F0"}},
        {"SSK", {3, "EC"}},
        {"STA", {3, "0C"}},
        {"STB", {3, "78"}},
        {"STCH", {3, "54"}},
        {"STF", {3, "80"}},
        {"STI", {3, "D4"}},
        {"STL", {3, "14"}},
        {"STS", {3, "7C"}},
        {"STSW", {3, "E8"}},
        {"STT", {3, "84"}},
        {"STX", {3, "10"}},
        {"SUB", {3, "1C"}},
        {"SUBF", {3, "5C"}},
        {"SUBR", {2, "94"}},
        {"SVC", {2, "B0"}},
        {"TD", {3, "E0"}},
        {"TIO", {1, "F8"}},
        {"TIX", {3, "2C"}},
        {"TIXR", {2, "B8"}},
        {"WD", {3, "DC"}}
    };
    map<string, string> REGTAB = {
        {"A", "0"},
        {"X", "1"},
        {"L", "2"},
        {"B", "3"},
        {"S", "4"},
        {"T", "5"},
        {"F", "6"},
        {"PC", "8"},
        {"SW", "9"},
    };
    map<string, int> SYMTAB;

    vector<LINE> LINES;
    vector<int> MODIFY;
    vector<pair<int, string>> OBJ;

    string program_name;
    int starting_address = 0;
    int program_length = 0;

    int LOCCTR = 0;
    int PC = 0;
    int BASE = 0;
    int INDX = 0;

    public:
        Assembler(string filename) {
            input_file.open(filename);

            if(input_file.fail()) {
                cout << "File cannot be opened." << endl;
                exit(1);
            }

            // optab_gen();
        }

        void run() {
            pass_1();
            pass_2();
            intermediate();
            generate_object_program();
            done();
        }
    
    private:
        void optab_gen() {
            freopen("instructions.txt", "r", stdin);

            string opcode;
            int format;
            string hexstring;

            for(int i = 0; i < 59; i++) {
                cin >> opcode >> format >> hexstring;
                OPTAB[opcode] = {format, hexstring};
            }

            fclose(stdin);
        }

        void done() {
            input_file.close();
        }

        void bind_val(string line, string *x, string *y, string *z) {
            /*
                Possible Line Example:
                1. ASSEMBLER_DIRECTIVE MNEMONIC TARGET_ADDRESS
                2. <spaces>            MNEMONIC TARGET_ADDRESS
                3. . this is a comment line 
                4. <spaces>            RSUB
            */
            string label;
            string mnemonic;
            string target;

            int count = 0;
            stringstream stream(line);
            string word;
            while(stream >> word)
                count++;

            // reset the steam
            stream.str(line);
            stream.clear();

            switch (count) {
                case 1: // type 4 -> RSUB
                    stream >> mnemonic;
                    label = "*";
                    target = "*";
                    break;
                case 2: // type 2 -> CLEAR A
                    stream >> mnemonic >> target;
                    label = "*";
                    break;
                case 3: // type 1 -> FIRST STL RETADR
                    stream >> label >> mnemonic >> target;
                    break;
                default: // C'hello world'
                    stream >> label >> mnemonic >> target;
                    getline(stream, word); // read the rest of the stream
                    target += word;
                    break;
            }

            *x = label;
            *y = mnemonic;
            *z = target;
        }

        int byte_size(int n) {
            string str = LINES[n].target;
            int res;
            switch (str[0]) {
                case 'C':
                    res = (str.substr(2, str.size() - 3)).length();
                    break;
                case 'X': // X'F1' -> size: 1
                    res = 1; // SUS ??
                    break;
                default: // maybe an error?
                    cout << "error: unknown byte string format" << endl;
                    exit(1);
                    break;
            }
            return res;
        }

        /*
        1 byte format
            |op(8 bits)|
        2 byte format
            |r1(8 bits)|r2(8 bits)|
        3 byte format
            |op(6 bits)|nixbpe|disp(12 bits)|
        4 byte format
            |op(6 bits)|nixbpe|address(20 bits)|
        */

        string fmt_2(int i) {
            // ADDR r1, r2
            string op;
            string r1;
            string r2 = "A";

            op = LINES[i].target; // r1, r2
            int j;
            for (j = 0; j < op.size() && op[j] != ','; j++)
                ;
            r1 = op.substr(0, j);
            if (j < op.size()) 
                r2 = op.substr(j + 1, op.size() - j - 1);

            string res;
            res = OPTAB[LINES[i].mnemonic].opcode; // the OPERATION's hexcode
            res += REGTAB[r1];
            res += REGTAB[r2];

            return res;
        }

        string fmt_3(int i) {
            /*
                Possible Value
                1. <MNE> <SYM> -> neither indirect nor immediate
                2. <MNE> <SYM>,X -> index addressing mode
                3. <MNE> <SYM>
                4. <MNE> #<SYM> -> immediate addressing mode
                |OPCODE|N|I|X|B|P|E|DISP|
                |6     | | | | | | |12  |
            */
            string str = LINES[i].target; 
            string disp; 
            string objcode;
            int nixbpe[6] = {0, 0, 0, 0, 0, 0};
            bool dr = 0;

            nixbpe[5] = 0; // not format 4
            
            // ADD <SYM>,X
            if(str.size() > 2 && str.substr(str.size() - 2) == ",X") { // index addressing mode
                str = str.substr(0, str.size() - 2); // -> TABLE
                // |N|I|X|B|P|E|
                // | | |1| | | |
                nixbpe[2] = 1; // index addressing mode
            }

            if(str[0] == '#') {
                // LDA #0 or LDB #TOTAL
                // immediate addressing mode
                // no displacement
                // |N|I|X|B|P|E|
                // |0|1| | | | |
                str = str.substr(1, str.size() - 1); // -> 0 or TOTAL
                nixbpe[1] = 1;
                if(SYMTAB.find(str) != SYMTAB.end()) { // TOTAL
                    disp = int2hex(SYMTAB[str], 3);
                }else { // 0
                    disp = str;
                    dr = 1;
                }
            }else if(str[0] == '@') { // indirect addressing
                // |N|I|X|B|P|E|
                // |1|0| | | | |
                nixbpe[0] = 1;
                str = str.substr(1, str.size() - 1);
                int z = SYMTAB[str], j;
                for(j = 0; j < LINES.size(); j++)
                    if(LINES[j].locctr == z)
                        break;
                disp = str;
                str = int2hex(LINES[j].locctr, 3);
                if(LINES[j].mnemonic != "WORD" && LINES[j].mnemonic != "BYTE" && LINES[j].mnemonic != "RESW" && LINES[j].mnemonic != "RESB") {
                    str = LINES[j].target;
                    z = SYMTAB[str];
                    for (j = 0; j < LINES.size(); j++)
                        if (LINES[j].label == disp)
                            break;
                    str = LINES[j].target;
                    disp = int2hex(SYMTAB[str], 3);
                }else {
                    disp = str;
                }
            }else if(str[0] == '=') { // literal =X'05'
                str = str.substr(3, str.size() - 4); // -> 05
                dr = 1;
            }else {
                // neither indirect nor immediate -> Simple addressing mode
                // both 1 or both 0
                // |N|I|X|B|P|E|
                // |1|1| | | | |
                disp = int2hex(SYMTAB[str], 3);
                nixbpe[0] = 1;
                nixbpe[1] = 1;
            }

            if(dr != 1 && str != "*") {
                int hex_int = hex2int(disp);
                if (hex_int - PC > -2048 && hex_int - PC < 2047) { // PC relative mode
                    nixbpe[4] = 1; // PC relative mode
                    disp = int2hex(hex_int - PC, 3);
                }else { // BASE relative mode
                    nixbpe[3] = 1; // BASE relative mode
                    disp = int2hex(hex_int - BASE, 3);
                }
                disp = disp.substr(disp.size() - 3, 3);
            }
            if(nixbpe[2] == 1) { // index addressing mode
                disp = int2hex(hex2int(disp) - INDX, 3);
            }
            while(disp.size() < 3)
                disp = "0" + disp;

            objcode = int2hex(hex2int(OPTAB[LINES[i].mnemonic].opcode) + (nixbpe[0] * 2) + (nixbpe[1] * 1), 2) +
                      bin2hex(nixbpe[2], nixbpe[3], nixbpe[4], nixbpe[5]) + 
                      disp;
            return objcode;
        }

        string fmt_4(int i) {
            //  |OPCODE|N|I|X|B|P|E|DISP|
            //  |6     | | | | | | |12  |
            string str = LINES[i].target;
            string mne = LINES[i].mnemonic;
            string TA = "";
            int nixbpe[6] = {0, 0, 0, 0, 0, 0};
            nixbpe[0] = (str[0] == '@');
            nixbpe[1] = (str[0] == '#'); // immediate address mode
            if (nixbpe[0] == nixbpe[1]) {
                nixbpe[0] = !nixbpe[0];
                nixbpe[1] = !nixbpe[1];
            }
            nixbpe[2] = (str.size() > 2 && str.substr(str.size() - 2) == ",X") ? 1 : 0;
            nixbpe[3] = 0; // format 4 don't need base relative
            nixbpe[4] = 0; // format 4 don't need pc relative
            nixbpe[5] = 1; // format 4 duhh

            if(str[0] == '@' || str[0] == '#')
                str = str.substr(1, str.length() - 1);

            if(str[str.length() - 1] == 'X' && str[str.length() - 2] == ',')
                str = str.substr(0, str.length() - 2);

            if(nixbpe[0] == 1 && nixbpe[1] == 1) {
                // neither indirect nor immediate -> Simple addressing mode
                // both 1 or both 0
                // |N|I|X|B|P|E|
                // |1|1| | | | |
                string s = int2hex(SYMTAB[str], 5);
                for (int j = 0; j < LINES.size(); j++) {
                    if (int2hex(LINES[j].locctr, 5) == s) {
                        if (nixbpe[2] == 0)
                            TA = s;
                        else
                            TA = int2hex(hex2int(s) + INDX, 5);
                    }
                }
            }else if(nixbpe[0] == 1 && nixbpe[1] == 0 && nixbpe[2] == 0) {
                // |N|I|X|B|P|E|
                // |1|0| | | | |
                string s = to_string(SYMTAB[str]);
                for (int j = 0; j < LINES.size(); j++)
                    if (to_string(LINES[j].locctr) == s) {
                        s = LINES[j].target;
                        for (int k = 0; j < LINES.size(); k++)
                            if (to_string(LINES[k].locctr) == s)
                                TA = LINES[k].target;
                    }
            }else if (nixbpe[0] == 0 && nixbpe[1] == 1) {
                // |N|I|X|B|P|E|
                // |0|1| | | | |
                if (str[0] < 65)
                    TA = int2hex(stoi(str), 5);
                else
                    TA = int2hex(SYMTAB[str], 5);
            }

            string objcode = OPTAB[LINES[i].mnemonic.substr(1)].opcode;

            objcode = int2hex(hex2int(objcode) + nixbpe[0] * 2 + nixbpe[1] * 1, 2) + 
                      bin2hex(nixbpe[2], nixbpe[3], nixbpe[4], nixbpe[5]) + 
                      TA;

            return objcode;
        }

        string int2hex(int num, int width) {
            string hex_str;
            stringstream int_to_hex;

            // set stream output to hex with width padded with 0
            int_to_hex << setfill('0') << setw(width) << hex << (int)num;
            hex_str = int_to_hex.str();
            int_to_hex.clear();

            // convert hex string to uppercase
            for(int i = 0; i < hex_str.length(); i++)
                hex_str[i] = toupper(hex_str[i]); 
            return hex_str;
        }

        int hex2int(string hexstring) {
            int number = (int)strtol(hexstring.c_str(), NULL, 16);
            return number;
        }

        string bin2hex(int a, int b, int c, int d) {
            string s;
            int sum = 0;
            sum += a * 8;
            sum += b * 4;
            sum += c * 2;
            sum += d * 1;
            s = int2hex(sum, 1);
            return s;
        }

        void intermediate() {
            for (int i = 0; i < LINES.size(); i++) {
                if(LINES[i].label == "*") {
                    if(LINES[i].target.length() == 8) // wtf is this??
                        cout << int2hex(LINES[i].locctr, 4) << "\t\t" << LINES[i].mnemonic << "\t" << LINES[i].target << "  " << LINES[i].object_code << endl;
                    else if(LINES[i].target == "*") // RSUB
                        cout << int2hex(LINES[i].locctr, 4) << "\t\t" << LINES[i].mnemonic << "\t \t  " << LINES[i].object_code << endl;
                    else
                        cout << int2hex(LINES[i].locctr, 4) << "\t\t" << LINES[i].mnemonic << "\t" << LINES[i].target << "\t  " << LINES[i].object_code << endl;
                }else
                    cout << int2hex(LINES[i].locctr, 4) << "\t" << LINES[i].label << "\t" << LINES[i].mnemonic << "\t" << LINES[i].target << "\t  " << LINES[i].object_code << endl;

                if(LINES[i].object_code.length() != 0)
                    OBJ.push_back({LINES[i].locctr, LINES[i].object_code});
            }
        }

        void generate_object_program() {
            freopen("output.txt", "w", stdout);
            int size = OBJ.size();
            cout << "H^" << program_name << " ^" << int2hex(starting_address, 6) << "^" << int2hex(program_length, 6) << endl;

            for (int i = 0; i < OBJ.size(); i += 5) {
                long long sum = 0;
                for (int j = i; j < i + min(size - i, 5); j++)
                    sum += OBJ[j].second.size() / 2;
                cout << "T^" << int2hex(OBJ[i].first, 6) << "^" << int2hex(sum, 2);

                for (int j = i; j < i + min(size - i, 5); j++)
                    cout << "^" << OBJ[j].second;
                cout << endl;
            }
            for (int i = 0; i < MODIFY.size(); i++)
                cout << "M^" << int2hex(MODIFY[i] + 1, 6) << "^05" << endl;
            cout << "E^" << int2hex(starting_address, 6);
        }

        void pass_1() {
            string line;
            string label, mnemonic, target;

            // Read First Input Line
            getline(input_file, line);
            bind_val(line, &label, &mnemonic, &target);

            if(mnemonic == "START") {
                program_name = label;
                LOCCTR = hex2int(target);
                starting_address = LOCCTR;
            }else {
                LOCCTR = 0;
            }

            while(getline(input_file, line)) {
                // comment, ignored by assembler
                if(line[0] == '.' || line == "") 
                    continue; 

                // Parse the line
                bind_val(line, &label, &mnemonic, &target);

                LINES.push_back({LOCCTR, label, mnemonic, target});

                if(label != "*") { // have ASSEMBLER DIRECTIVE, add to SYMBOL TABLE
                    if(SYMTAB.find(label) != SYMTAB.end()) {
                        cout << "Error: duplicate label" << endl;
                        exit(1);
                    }
                    SYMTAB[label] = LOCCTR;
                }

                if(mnemonic == "WORD")
                    LOCCTR += 3; // size of a word
                else if(mnemonic == "RESW")
                    LOCCTR +=  3 * stoi(target);
                else if(mnemonic == "RESB") // RESB 4096(10進位)
                    LOCCTR += stoi(target);
                else if(mnemonic == "BYTE")
                    LOCCTR += byte_size(LINES.size() - 1);
                else if(mnemonic[0] == '+') // extended format
                    LOCCTR += 4;
                else if(OPTAB.find(mnemonic) != OPTAB.end())
                    LOCCTR += OPTAB[mnemonic].format; // add instruction length to LOCCTR
            }
            program_length = LOCCTR - starting_address;
        }

        void pass_2() {
            /*
                Possible Line Example:
                1. LABEL    MNEMONIC TARGET_ADDRESS
                2. <spaces> MNEMONIC TARGET_ADDRESS
                3. . this is a comment line 
                4. <spaces>RSUB

                PROG pair<int, vector<string>>
                OPTAB map<string, pair<int, string>>
            */
            for(int i = 0; i < LINES.size(); i++) {
                PC = LINES[i + 1].locctr; // get the LOCCTR

                bool fmt4 = (LINES[i].mnemonic)[0] == '+' ? true : false; // get the MNEMONIC
                int format = OPTAB[LINES[i].mnemonic].format; // get the MNEMONIC INSTRUCTION's format

                string OBJ_CODE;
                switch(format) {
                    case 1:
                        OBJ_CODE = OPTAB[LINES[i].mnemonic].opcode; // instruction's HEXCODE
                        break;
                    case 2:
                        OBJ_CODE = fmt_2(i);
                        break;
                    case 3:
                        // LDA #0 -> type 3
                        // LDA X -> type 2
                        OBJ_CODE = fmt_3(i);
                        break;
                    default:
                        if(fmt4) { // 一頭霧水
                            OBJ_CODE = fmt_4(i);
                            // # 不是immediate value而已嗎？
                            bool flag = (LINES[i].target)[0] != '#' ? true : false;
                            if(flag) MODIFY.push_back(LINES[i].locctr);
                        }
                        break;
                }

                if(LINES[i].mnemonic == "BASE") {
                    BASE = SYMTAB[LINES[i].target]; // assign BASE ADDRESS with LOCCTR of LABEL
                }else if(LINES[i].mnemonic == "BYTE") { // convert constant to object code
                    // EOF	    BYTE	C'EOF'
                    // OUTPUT	BYTE	X'05'
                    string str = LINES[i].target;
                    OBJ_CODE = "";
                    if(str[0] == 'C') {
                        str = str.substr(2, str.size() - 3); // extract the inner parenthesis
                        // C'EOF' -> 454F46
                        for(int j = 0; j < str.size(); j++) { // convert each INT to HEX
                            OBJ_CODE += int2hex(str[j], 2);
                        }
                    }else // X'F1' already hex
                        OBJ_CODE = str.substr(2, str.size() - 3); // extract the inner parenthesis
                }
                LINES[i].object_code = OBJ_CODE; // directly append to the vector
            }
        }
};

int main(int argc, char** argv) {
    if(argc != 2) { // no argument given
        cout << "Usage: <program_name> <input_file>" << endl;
        cout << "Terminating..." << endl;
        return 1;
    }

    Assembler assem(argv[1]);

    assem.run();

    return 0;
}