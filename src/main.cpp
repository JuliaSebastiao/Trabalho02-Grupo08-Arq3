#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

enum class OpType {
    Load,
    Store,
    Add,
    Sub,
    Mul,
    Div
};

enum class StationKind {
    Load,
    Store,
    Add,
    Mult
};

struct Config {
    int robEntries = 6;
    int loadBuffers = 2;
    int storeBuffers = 2;
    int addStations = 3;
    int multStations = 2;
    int issueWidth = 1;
    int commitWidth = 1;
    int cdbWidth = 1;

    int loadLatency = 2;
    int storeLatency = 2;
    int addLatency = 2;
    int subLatency = 2;
    int mulLatency = 10;
    int divLatency = 40;

    int loadUnits = 1;
    int storeUnits = 1;
    int addUnits = 1;
    int multUnits = 1;
};

struct Instruction {
    int index = -1;
    OpType op = OpType::Add;
    string text;
    int dstFp = -1;
    int srcFp1 = -1;
    int srcFp2 = -1;
    int baseReg = -1;
    long long offset = 0;
};

struct RegisterTag {
    bool busy = false;
    int rob = -1;
};

struct InstStatus {
    string text;
    int issue = -1;
    int execStart = -1;
    int execEnd = -1;
    int writeResult = -1;
    int commit = -1;
};

struct RobEntry {
    int id = -1;
    bool busy = false;
    bool ready = false;
    OpType op = OpType::Add;
    int instrIndex = -1;
    int dstFp = -1;
    long long address = 0;
    bool hasAddress = false;
    double value = 0.0;
    string instruction;
    string state;
    string destination;
    string valueText;
};

struct Station {
    string name;
    StationKind kind = StationKind::Add;
    bool busy = false;
    OpType op = OpType::Add;
    int instrIndex = -1;
    int rob = -1;
    int issueCycle = -1;

    bool hasVj = false;
    bool hasVk = false;
    double vj = 0.0;
    double vk = 0.0;
    int qj = -1;
    int qk = -1;

    string addressText;
    long long address = 0;

    bool executing = false;
    bool completed = false;
    int remaining = 0;
    int completeCycle = -1;
    double result = 0.0;
    string resultText;
};

enum class ExpectKind {
    FpRegister,
    IntRegister,
    Memory,
    Cycles,
    InstructionIssue,
    InstructionExecute,
    InstructionWrite,
    InstructionCommit
};

struct Expectation {
    ExpectKind kind = ExpectKind::FpRegister;
    int index = -1;
    long long address = 0;
    double number = 0.0;
    double secondNumber = -1.0;
    string label;
    int lineNo = -1;
};

struct ProgramInput {
    Config config;
    vector<Instruction> instructions;
    array<double, 32> fp{};
    array<long long, 32> integer{};
    unordered_map<long long, double> memory;
    set<int> usedFp;
    set<int> usedInt;
    set<long long> usedMem;
    vector<Expectation> expectations;
};

static string trim(const string &s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static string upper(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(toupper(c));
    });
    return s;
}

static string removeDots(string s) {
    s.erase(remove(s.begin(), s.end(), '.'), s.end());
    return s;
}

static vector<string> splitWords(string s) {
    for (char &c : s) {
        if (c == '=' || c == ':' || c == ',') {
            c = ' ';
        }
    }
    stringstream ss(s);
    vector<string> out;
    string token;
    while (ss >> token) {
        out.push_back(token);
    }
    return out;
}

static string fpReg(int n) {
    return "F" + to_string(n);
}

static string intReg(int n) {
    return "R" + to_string(n);
}

static string formatDouble(double value) {
    if (isnan(value)) {
        return "NaN";
    }
    if (isinf(value)) {
        return value > 0 ? "Inf" : "-Inf";
    }

    double rounded = round(value);
    if (fabs(value - rounded) < 1e-9) {
        return to_string(static_cast<long long>(rounded));
    }

    ostringstream oss;
    oss << fixed << setprecision(6) << value;
    string s = oss.str();
    while (!s.empty() && s.back() == '0') {
        s.pop_back();
    }
    if (!s.empty() && s.back() == '.') {
        s.pop_back();
    }
    return s;
}

static string robTag(int id) {
    return id < 0 ? "" : "#" + to_string(id);
}

static string opMnemonic(OpType op) {
    switch (op) {
    case OpType::Load:
        return "L.D";
    case OpType::Store:
        return "S.D";
    case OpType::Add:
        return "ADD.D";
    case OpType::Sub:
        return "SUB.D";
    case OpType::Mul:
        return "MUL.D";
    case OpType::Div:
        return "DIV.D";
    }
    return "";
}

static string opShort(OpType op) {
    switch (op) {
    case OpType::Load:
        return "Load";
    case OpType::Store:
        return "Store";
    case OpType::Add:
        return "ADD";
    case OpType::Sub:
        return "SUB";
    case OpType::Mul:
        return "MUL";
    case OpType::Div:
        return "DIV";
    }
    return "";
}

static StationKind stationKindFor(OpType op) {
    switch (op) {
    case OpType::Load:
        return StationKind::Load;
    case OpType::Store:
        return StationKind::Store;
    case OpType::Add:
    case OpType::Sub:
        return StationKind::Add;
    case OpType::Mul:
    case OpType::Div:
        return StationKind::Mult;
    }
    return StationKind::Add;
}

static string stationKindName(StationKind kind) {
    switch (kind) {
    case StationKind::Load:
        return "Load";
    case StationKind::Store:
        return "Store";
    case StationKind::Add:
        return "Add";
    case StationKind::Mult:
        return "Mult";
    }
    return "";
}

static void validateRegister(int reg, const string &kind, int lineNo) {
    if (reg < 0 || reg > 31) {
        throw runtime_error("Linha " + to_string(lineNo) + ": registrador " + kind + to_string(reg) + " invalido.");
    }
}

static bool parseMnemonic(const string &raw, OpType &op) {
    string key = removeDots(upper(raw));
    if (key == "LD" || key == "L" || key == "LOAD") {
        op = OpType::Load;
        return true;
    }
    if (key == "SD" || key == "S" || key == "STORE") {
        op = OpType::Store;
        return true;
    }
    if (key == "ADD" || key == "ADDD") {
        op = OpType::Add;
        return true;
    }
    if (key == "SUB" || key == "SUBD") {
        op = OpType::Sub;
        return true;
    }
    if (key == "MUL" || key == "MULD" || key == "MULT" || key == "MULTD") {
        op = OpType::Mul;
        return true;
    }
    if (key == "DIV" || key == "DIVD") {
        op = OpType::Div;
        return true;
    }
    return false;
}

static string buildInstructionText(const Instruction &inst) {
    if (inst.op == OpType::Load || inst.op == OpType::Store) {
        int fp = inst.op == OpType::Load ? inst.dstFp : inst.srcFp1;
        return opMnemonic(inst.op) + " " + fpReg(fp) + ", " + to_string(inst.offset) + "(" + intReg(inst.baseReg) + ")";
    }
    return opMnemonic(inst.op) + " " + fpReg(inst.dstFp) + ", " + fpReg(inst.srcFp1) + ", " + fpReg(inst.srcFp2);
}

static bool isKnownConfigName(const string &raw) {
    string name = upper(raw);
    return name == "ROB_ENTRIES" || name == "ROB" ||
           name == "LOAD_BUFFERS" || name == "LOAD_RS" ||
           name == "STORE_BUFFERS" || name == "STORE_RS" ||
           name == "ADD_STATIONS" || name == "ADD_RS" ||
           name == "MULT_STATIONS" || name == "MULT_RS" ||
           name == "ISSUE_WIDTH" || name == "COMMIT_WIDTH" || name == "CDB_WIDTH" ||
           name == "LOAD_LATENCY" || name == "LATENCY_LOAD" ||
           name == "STORE_LATENCY" || name == "LATENCY_STORE" ||
           name == "ADD_LATENCY" || name == "LATENCY_ADD" ||
           name == "SUB_LATENCY" || name == "LATENCY_SUB" ||
           name == "MUL_LATENCY" || name == "MULT_LATENCY" || name == "LATENCY_MUL" || name == "LATENCY_MULT" ||
           name == "DIV_LATENCY" || name == "LATENCY_DIV" ||
           name == "LOAD_UNITS" || name == "STORE_UNITS" || name == "ADD_UNITS" || name == "MULT_UNITS";
}

static void applyConfig(Config &config, const string &rawName, int value, int lineNo) {
    if (value <= 0) {
        throw runtime_error("Linha " + to_string(lineNo) + ": configuracao deve ser maior que zero.");
    }

    string name = upper(rawName);
    if (name == "ROB_ENTRIES" || name == "ROB") {
        config.robEntries = value;
    } else if (name == "LOAD_BUFFERS" || name == "LOAD_RS") {
        config.loadBuffers = value;
    } else if (name == "STORE_BUFFERS" || name == "STORE_RS") {
        config.storeBuffers = value;
    } else if (name == "ADD_STATIONS" || name == "ADD_RS") {
        config.addStations = value;
    } else if (name == "MULT_STATIONS" || name == "MULT_RS") {
        config.multStations = value;
    } else if (name == "ISSUE_WIDTH") {
        config.issueWidth = value;
    } else if (name == "COMMIT_WIDTH") {
        config.commitWidth = value;
    } else if (name == "CDB_WIDTH") {
        config.cdbWidth = value;
    } else if (name == "LOAD_LATENCY" || name == "LATENCY_LOAD") {
        config.loadLatency = value;
    } else if (name == "STORE_LATENCY" || name == "LATENCY_STORE") {
        config.storeLatency = value;
    } else if (name == "ADD_LATENCY" || name == "LATENCY_ADD") {
        config.addLatency = value;
    } else if (name == "SUB_LATENCY" || name == "LATENCY_SUB") {
        config.subLatency = value;
    } else if (name == "MUL_LATENCY" || name == "MULT_LATENCY" || name == "LATENCY_MUL" || name == "LATENCY_MULT") {
        config.mulLatency = value;
    } else if (name == "DIV_LATENCY" || name == "LATENCY_DIV") {
        config.divLatency = value;
    } else if (name == "LOAD_UNITS") {
        config.loadUnits = value;
    } else if (name == "STORE_UNITS") {
        config.storeUnits = value;
    } else if (name == "ADD_UNITS") {
        config.addUnits = value;
    } else if (name == "MULT_UNITS") {
        config.multUnits = value;
    } else {
        throw runtime_error("Linha " + to_string(lineNo) + ": configuracao desconhecida '" + rawName + "'.");
    }
}

static string stripComment(string line) {
    size_t hash = line.find('#');
    size_t slash = line.find("//");
    size_t pos = string::npos;
    if (hash != string::npos) {
        pos = hash;
    }
    if (slash != string::npos) {
        pos = pos == string::npos ? slash : min(pos, slash);
    }
    if (pos != string::npos) {
        line = line.substr(0, pos);
    }
    return trim(line);
}

static ProgramInput parseInputFile(const string &path) {
    ifstream in(path);
    if (!in) {
        throw runtime_error("Nao foi possivel abrir o arquivo de entrada: " + path);
    }

    ProgramInput input;
    input.fp.fill(0.0);
    input.integer.fill(0);

    regex fpAssign(R"(^F([0-9]+)\s*=\s*([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?)$)", regex_constants::icase);
    regex intAssign(R"(^R([0-9]+)\s*=\s*([-+]?[0-9]+)$)", regex_constants::icase);
    regex memAssign(R"(^(MEM|M)\s*\[\s*([-+]?[0-9]+)\s*\]\s*=\s*([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?)$)", regex_constants::icase);
    regex expectFp(R"(^EXPECT\s+F([0-9]+)\s*=\s*([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?)$)", regex_constants::icase);
    regex expectInt(R"(^EXPECT\s+R([0-9]+)\s*=\s*([-+]?[0-9]+)$)", regex_constants::icase);
    regex expectMem(R"(^EXPECT\s+(MEM|M)\s*\[\s*([-+]?[0-9]+)\s*\]\s*=\s*([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?)$)", regex_constants::icase);
    regex expectCycles(R"(^EXPECT\s+CYCLES\s*=\s*([0-9]+)$)", regex_constants::icase);
    regex expectInstrCycle(R"(^EXPECT\s+(ISSUE|EXECUTE|WRITE|WRITE_RESULT|COMMIT)\s+([0-9]+)\s*=\s*([0-9]+)(\s*-\s*([0-9]+))?$)", regex_constants::icase);
    regex memInstr(R"(^([A-Za-z.]+)\s+F([0-9]+)\s*,\s*([-+]?[0-9]+)\s*\(\s*R([0-9]+)\s*\)$)", regex_constants::icase);
    regex aluInstr(R"(^([A-Za-z.]+)\s+F([0-9]+)\s*,\s*F([0-9]+)\s*,\s*F([0-9]+)\s*$)", regex_constants::icase);

    string line;
    int lineNo = 0;
    while (getline(in, line)) {
        ++lineNo;
        line = stripComment(line);
        if (line.empty()) {
            continue;
        }

        vector<string> tokens = splitWords(line);
        if (!tokens.empty()) {
            string first = upper(tokens[0]);
            if (first == ".CONFIG" || first == "CONFIG") {
                if (tokens.size() != 3) {
                    throw runtime_error("Linha " + to_string(lineNo) + ": use CONFIG NOME VALOR.");
                }
                applyConfig(input.config, tokens[1], stoi(tokens[2]), lineNo);
                continue;
            }
            if (isKnownConfigName(tokens[0])) {
                if (tokens.size() != 2) {
                    throw runtime_error("Linha " + to_string(lineNo) + ": use NOME_CONFIG VALOR.");
                }
                applyConfig(input.config, tokens[0], stoi(tokens[1]), lineNo);
                continue;
            }
        }

        smatch match;
        if (regex_match(line, match, fpAssign)) {
            int reg = stoi(match[1]);
            validateRegister(reg, "F", lineNo);
            input.fp[reg] = stod(match[2]);
            input.usedFp.insert(reg);
            continue;
        }
        if (regex_match(line, match, intAssign)) {
            int reg = stoi(match[1]);
            validateRegister(reg, "R", lineNo);
            input.integer[reg] = stoll(match[2]);
            input.usedInt.insert(reg);
            continue;
        }
        if (regex_match(line, match, memAssign)) {
            long long address = stoll(match[2]);
            input.memory[address] = stod(match[3]);
            input.usedMem.insert(address);
            continue;
        }
        if (regex_match(line, match, expectFp)) {
            int reg = stoi(match[1]);
            validateRegister(reg, "F", lineNo);
            Expectation expectation;
            expectation.kind = ExpectKind::FpRegister;
            expectation.index = reg;
            expectation.number = stod(match[2]);
            expectation.label = fpReg(reg);
            expectation.lineNo = lineNo;
            input.usedFp.insert(reg);
            input.expectations.push_back(expectation);
            continue;
        }
        if (regex_match(line, match, expectInt)) {
            int reg = stoi(match[1]);
            validateRegister(reg, "R", lineNo);
            Expectation expectation;
            expectation.kind = ExpectKind::IntRegister;
            expectation.index = reg;
            expectation.number = static_cast<double>(stoll(match[2]));
            expectation.label = intReg(reg);
            expectation.lineNo = lineNo;
            input.usedInt.insert(reg);
            input.expectations.push_back(expectation);
            continue;
        }
        if (regex_match(line, match, expectMem)) {
            long long address = stoll(match[2]);
            Expectation expectation;
            expectation.kind = ExpectKind::Memory;
            expectation.address = address;
            expectation.number = stod(match[3]);
            expectation.label = "Mem[" + to_string(address) + "]";
            expectation.lineNo = lineNo;
            input.usedMem.insert(address);
            input.expectations.push_back(expectation);
            continue;
        }
        if (regex_match(line, match, expectCycles)) {
            Expectation expectation;
            expectation.kind = ExpectKind::Cycles;
            expectation.number = static_cast<double>(stoi(match[1]));
            expectation.label = "Cycles";
            expectation.lineNo = lineNo;
            input.expectations.push_back(expectation);
            continue;
        }
        if (regex_match(line, match, expectInstrCycle)) {
            string field = upper(match[1]);
            int instructionNumber = stoi(match[2]);
            if (instructionNumber <= 0) {
                throw runtime_error("Linha " + to_string(lineNo) + ": numero da instrucao deve comecar em 1.");
            }

            Expectation expectation;
            if (field == "ISSUE") {
                expectation.kind = ExpectKind::InstructionIssue;
                expectation.label = "Issue I" + to_string(instructionNumber);
            } else if (field == "EXECUTE") {
                expectation.kind = ExpectKind::InstructionExecute;
                expectation.label = "Execute I" + to_string(instructionNumber);
            } else if (field == "WRITE" || field == "WRITE_RESULT") {
                expectation.kind = ExpectKind::InstructionWrite;
                expectation.label = "Write result I" + to_string(instructionNumber);
            } else {
                expectation.kind = ExpectKind::InstructionCommit;
                expectation.label = "Commit I" + to_string(instructionNumber);
            }
            expectation.index = instructionNumber - 1;
            expectation.number = static_cast<double>(stoi(match[3]));
            expectation.secondNumber = match[5].matched ? static_cast<double>(stoi(match[5])) : expectation.number;
            expectation.lineNo = lineNo;
            input.expectations.push_back(expectation);
            continue;
        }

        if (regex_match(line, match, memInstr)) {
            OpType op;
            if (!parseMnemonic(match[1], op) || (op != OpType::Load && op != OpType::Store)) {
                throw runtime_error("Linha " + to_string(lineNo) + ": instrucao de memoria invalida.");
            }

            Instruction inst;
            inst.index = static_cast<int>(input.instructions.size());
            inst.op = op;
            inst.offset = stoll(match[3]);
            inst.baseReg = stoi(match[4]);
            validateRegister(inst.baseReg, "R", lineNo);
            int fp = stoi(match[2]);
            validateRegister(fp, "F", lineNo);

            if (op == OpType::Load) {
                inst.dstFp = fp;
                input.usedFp.insert(inst.dstFp);
            } else {
                inst.srcFp1 = fp;
                input.usedFp.insert(inst.srcFp1);
            }
            input.usedInt.insert(inst.baseReg);
            input.usedMem.insert(inst.offset + input.integer[inst.baseReg]);
            inst.text = buildInstructionText(inst);
            input.instructions.push_back(inst);
            continue;
        }

        if (regex_match(line, match, aluInstr)) {
            OpType op;
            if (!parseMnemonic(match[1], op) || op == OpType::Load || op == OpType::Store) {
                throw runtime_error("Linha " + to_string(lineNo) + ": instrucao aritmetica invalida.");
            }

            Instruction inst;
            inst.index = static_cast<int>(input.instructions.size());
            inst.op = op;
            inst.dstFp = stoi(match[2]);
            inst.srcFp1 = stoi(match[3]);
            inst.srcFp2 = stoi(match[4]);
            validateRegister(inst.dstFp, "F", lineNo);
            validateRegister(inst.srcFp1, "F", lineNo);
            validateRegister(inst.srcFp2, "F", lineNo);
            input.usedFp.insert(inst.dstFp);
            input.usedFp.insert(inst.srcFp1);
            input.usedFp.insert(inst.srcFp2);
            inst.text = buildInstructionText(inst);
            input.instructions.push_back(inst);
            continue;
        }

        throw runtime_error("Linha " + to_string(lineNo) + ": comando nao reconhecido: " + line);
    }

    if (input.instructions.empty()) {
        throw runtime_error("Arquivo de entrada nao contem instrucoes.");
    }

    for (const Expectation &expectation : input.expectations) {
        bool isInstructionExpectation =
            expectation.kind == ExpectKind::InstructionIssue ||
            expectation.kind == ExpectKind::InstructionExecute ||
            expectation.kind == ExpectKind::InstructionWrite ||
            expectation.kind == ExpectKind::InstructionCommit;
        if (isInstructionExpectation &&
            (expectation.index < 0 || expectation.index >= static_cast<int>(input.instructions.size()))) {
            throw runtime_error("Linha " + to_string(expectation.lineNo) +
                                ": EXPECT referencia instrucao inexistente.");
        }
    }

    return input;
}

class TomasuloSimulator {
  public:
    explicit TomasuloSimulator(ProgramInput input)
        : config(input.config),
          program(std::move(input.instructions)),
          fp(input.fp),
          integer(input.integer),
          memory(std::move(input.memory)),
          usedFp(std::move(input.usedFp)),
          usedInt(std::move(input.usedInt)),
          usedMem(std::move(input.usedMem)),
          expectations(std::move(input.expectations)) {
        status.resize(program.size());
        for (size_t i = 0; i < program.size(); ++i) {
            status[i].text = program[i].text;
        }

        fpStatus.fill(RegisterTag{});
        rob.resize(config.robEntries);
        for (int i = 0; i < config.robEntries; ++i) {
            rob[i].id = i + 1;
        }

        addStations(StationKind::Load, "Load", config.loadBuffers);
        addStations(StationKind::Store, "Store", config.storeBuffers);
        addStations(StationKind::Add, "Add", config.addStations);
        addStations(StationKind::Mult, "Mult", config.multStations);
    }

    bool run(bool trace, bool step, int maxCycles) {
        int cycle = 1;
        while (!finished()) {
            if (cycle > maxCycles) {
                throw runtime_error("Execucao interrompida: limite de ciclos atingido.");
            }

            events.clear();
            commitPhase(cycle);
            executePhase(cycle);
            writeResultPhase(cycle);
            issuePhase(cycle);

            if (trace) {
                printCycle(cout, cycle);
                if (step) {
                    cout << "Pressione ENTER para avancar..." << flush;
                    string dummy;
                    getline(cin, dummy);
                }
            }
            ++cycle;
        }

        finalCycles = cycle - 1;
        cout << "\n=== Execucao finalizada em " << finalCycles << " ciclos ===\n";
        printInstructionStatus(cout);
        printFinalRegisters(cout);
        printFinalMemory(cout);
        return printValidationReport(cout);
    }

  private:
    Config config;
    vector<Instruction> program;
    vector<InstStatus> status;
    array<double, 32> fp{};
    array<long long, 32> integer{};
    unordered_map<long long, double> memory;
    set<int> usedFp;
    set<int> usedInt;
    set<long long> usedMem;
    vector<Expectation> expectations;
    array<RegisterTag, 32> fpStatus{};
    int finalCycles = 0;

    vector<RobEntry> rob;
    int robHead = 0;
    int robTail = 0;
    int robCount = 0;
    size_t pc = 0;

    vector<Station> stations;
    vector<string> events;

    void addStations(StationKind kind, const string &prefix, int count) {
        for (int i = 1; i <= count; ++i) {
            Station station;
            station.name = prefix + to_string(i);
            station.kind = kind;
            stations.push_back(station);
        }
    }

    bool finished() const {
        if (pc < program.size() || robCount > 0) {
            return false;
        }
        for (const Station &station : stations) {
            if (station.busy) {
                return false;
            }
        }
        return true;
    }

    int latencyFor(OpType op) const {
        switch (op) {
        case OpType::Load:
            return config.loadLatency;
        case OpType::Store:
            return config.storeLatency;
        case OpType::Add:
            return config.addLatency;
        case OpType::Sub:
            return config.subLatency;
        case OpType::Mul:
            return config.mulLatency;
        case OpType::Div:
            return config.divLatency;
        }
        return 1;
    }

    int unitsFor(StationKind kind) const {
        switch (kind) {
        case StationKind::Load:
            return config.loadUnits;
        case StationKind::Store:
            return config.storeUnits;
        case StationKind::Add:
            return config.addUnits;
        case StationKind::Mult:
            return config.multUnits;
        }
        return 1;
    }

    RobEntry &robById(int id) {
        return rob.at(id - 1);
    }

    const RobEntry &robById(int id) const {
        return rob.at(id - 1);
    }

    Station *findFreeStation(StationKind kind) {
        for (Station &station : stations) {
            if (!station.busy && station.kind == kind) {
                return &station;
            }
        }
        return nullptr;
    }

    string addressExpression(const Instruction &inst) const {
        return to_string(inst.offset) + " + Regs[" + intReg(inst.baseReg) + "]";
    }

    string memoryDestination(const Instruction &inst) const {
        return "Mem[" + addressExpression(inst) + "]";
    }

    void fillOperandFromFp(int fpRegIndex, bool &hasValue, double &value, int &tag) {
        if (fpStatus[fpRegIndex].busy) {
            int producer = fpStatus[fpRegIndex].rob;
            const RobEntry &entry = robById(producer);
            if (entry.busy && entry.ready) {
                hasValue = true;
                value = entry.value;
                tag = -1;
            } else {
                hasValue = false;
                tag = producer;
            }
        } else {
            hasValue = true;
            value = fp[fpRegIndex];
            tag = -1;
        }
    }

    void clearStation(Station &station) {
        string name = station.name;
        StationKind kind = station.kind;
        station = Station{};
        station.name = name;
        station.kind = kind;
    }

    void commitPhase(int cycle) {
        for (int committed = 0; committed < config.commitWidth && robCount > 0; ++committed) {
            RobEntry &entry = rob[robHead];
            if (!entry.busy || !entry.ready) {
                return;
            }

            if (entry.op == OpType::Store) {
                memory[entry.address] = entry.value;
                usedMem.insert(entry.address);
                events.push_back("Commit: ROB" + robTag(entry.id) + " gravou " + formatDouble(entry.value) +
                                 " em Mem[" + to_string(entry.address) + "].");
            } else {
                fp[entry.dstFp] = entry.value;
                usedFp.insert(entry.dstFp);
                if (fpStatus[entry.dstFp].busy && fpStatus[entry.dstFp].rob == entry.id) {
                    fpStatus[entry.dstFp] = RegisterTag{};
                }
                events.push_back("Commit: ROB" + robTag(entry.id) + " atualizou " + fpReg(entry.dstFp) +
                                 " = " + formatDouble(entry.value) + ".");
            }

            status[entry.instrIndex].commit = cycle;
            entry.busy = false;
            entry.ready = false;
            entry.state = "Commit";

            robHead = (robHead + 1) % config.robEntries;
            --robCount;
        }
    }

    bool stationReady(const Station &station) const {
        if (!station.busy || station.executing || station.completed) {
            return false;
        }
        if (station.op == OpType::Load) {
            return loadCanReadMemory(station);
        }
        if (station.op == OpType::Store) {
            return station.qj == -1 && station.hasVj;
        }
        return station.qj == -1 && station.qk == -1 && station.hasVj && station.hasVk;
    }

    bool loadCanReadMemory(const Station &station) const {
        for (const RobEntry &entry : rob) {
            if (!entry.busy || entry.op != OpType::Store) {
                continue;
            }
            if (entry.instrIndex < station.instrIndex && entry.hasAddress && entry.address == station.address) {
                return false;
            }
        }
        return true;
    }

    void executePhase(int cycle) {
        map<StationKind, int> running;
        for (const Station &station : stations) {
            if (station.busy && station.executing && !station.completed) {
                running[station.kind]++;
            }
        }

        vector<Station *> ordered;
        for (Station &station : stations) {
            if (stationReady(station) && station.issueCycle < cycle) {
                ordered.push_back(&station);
            }
        }
        sort(ordered.begin(), ordered.end(), [](const Station *a, const Station *b) {
            return a->instrIndex < b->instrIndex;
        });

        for (Station *station : ordered) {
            int available = unitsFor(station->kind);
            if (running[station->kind] >= available) {
                continue;
            }
            station->executing = true;
            station->remaining = latencyFor(station->op);
            running[station->kind]++;
            RobEntry &entry = robById(station->rob);
            if (entry.busy) {
                entry.state = "Execute";
            }
            if (status[station->instrIndex].execStart == -1) {
                status[station->instrIndex].execStart = cycle;
            }
            events.push_back("Execute: " + station->name + " iniciou " + opShort(station->op) + ".");
        }

        for (Station &station : stations) {
            if (!station.busy || !station.executing || station.completed) {
                continue;
            }

            station.remaining--;
            if (station.remaining <= 0) {
                station.executing = false;
                station.completed = true;
                station.completeCycle = cycle;
                computeResult(station);
                status[station.instrIndex].execEnd = cycle;
                events.push_back("Execute: " + station.name + " concluiu " + opShort(station.op) + ".");
            }
        }
    }

    void computeResult(Station &station) {
        switch (station.op) {
        case OpType::Load: {
            auto it = memory.find(station.address);
            station.result = it == memory.end() ? 0.0 : it->second;
            usedMem.insert(station.address);
            station.resultText = "Mem[" + to_string(station.address) + "] = " + formatDouble(station.result);
            break;
        }
        case OpType::Store:
            station.result = station.vj;
            station.resultText = formatDouble(station.result) + " -> Mem[" + to_string(station.address) + "]";
            break;
        case OpType::Add:
            station.result = station.vj + station.vk;
            station.resultText = formatDouble(station.result);
            break;
        case OpType::Sub:
            station.result = station.vj - station.vk;
            station.resultText = formatDouble(station.result);
            break;
        case OpType::Mul:
            station.result = station.vj * station.vk;
            station.resultText = formatDouble(station.result);
            break;
        case OpType::Div:
            if (fabs(station.vk) < 1e-12) {
                station.result = station.vj >= 0 ? numeric_limits<double>::infinity()
                                                 : -numeric_limits<double>::infinity();
                station.resultText = "divisao por zero -> " + formatDouble(station.result);
            } else {
                station.result = station.vj / station.vk;
                station.resultText = formatDouble(station.result);
            }
            break;
        }
    }

    void writeResultPhase(int cycle) {
        vector<Station *> completedStores;
        vector<Station *> completedForCdb;
        for (Station &station : stations) {
            if (!station.busy || !station.completed || station.completeCycle >= cycle) {
                continue;
            }
            if (station.op == OpType::Store) {
                completedStores.push_back(&station);
            } else {
                completedForCdb.push_back(&station);
            }
        }

        auto byAge = [](const Station *a, const Station *b) {
            return a->instrIndex < b->instrIndex;
        };
        sort(completedStores.begin(), completedStores.end(), byAge);
        sort(completedForCdb.begin(), completedForCdb.end(), byAge);

        for (Station *station : completedStores) {
            writeCompletedStation(*station, cycle, false);
        }

        int broadcasts = 0;
        for (Station *station : completedForCdb) {
            if (broadcasts >= config.cdbWidth) {
                break;
            }
            writeCompletedStation(*station, cycle, true);
            broadcasts++;
        }
    }

    void writeCompletedStation(Station &station, int cycle, bool broadcast) {
        RobEntry &entry = robById(station.rob);
        entry.value = station.result;
        entry.valueText = station.resultText;
        entry.ready = true;
        entry.state = "Write result";
        if (station.op == OpType::Store) {
            entry.address = station.address;
            entry.hasAddress = true;
        }
        status[station.instrIndex].writeResult = cycle;

        if (broadcast) {
            for (Station &waiting : stations) {
                if (!waiting.busy) {
                    continue;
                }
                if (waiting.qj == station.rob) {
                    waiting.qj = -1;
                    waiting.hasVj = true;
                    waiting.vj = station.result;
                }
                if (waiting.qk == station.rob) {
                    waiting.qk = -1;
                    waiting.hasVk = true;
                    waiting.vk = station.result;
                }
            }
            events.push_back("Write result/CDB: " + station.name + " publicou ROB" + robTag(station.rob) +
                             " = " + station.resultText + ".");
        } else {
            events.push_back("Write result: " + station.name + " deixou Store pronto no ROB" + robTag(station.rob) + ".");
        }

        clearStation(station);
    }

    void issuePhase(int cycle) {
        for (int issued = 0; issued < config.issueWidth && pc < program.size(); ++issued) {
            Instruction &inst = program[pc];
            StationKind kind = stationKindFor(inst.op);
            Station *station = findFreeStation(kind);
            if (station == nullptr) {
                events.push_back("Issue parado: nao ha estacao de reserva livre do tipo " + stationKindName(kind) + ".");
                return;
            }
            if (robCount >= config.robEntries) {
                events.push_back("Issue parado: Reorder buffer cheio.");
                return;
            }

            int robId = allocateRob(inst);
            fillStation(*station, inst, robId, cycle);
            status[inst.index].issue = cycle;
            events.push_back("Issue: " + inst.text + " entrou em " + station->name + " e ROB" + robTag(robId) + ".");
            ++pc;
        }
    }

    int allocateRob(const Instruction &inst) {
        RobEntry &entry = rob[robTail];
        int id = entry.id;
        entry = RobEntry{};
        entry.id = id;
        entry.busy = true;
        entry.ready = false;
        entry.op = inst.op;
        entry.instrIndex = inst.index;
        entry.dstFp = inst.dstFp;
        entry.instruction = inst.text;
        entry.state = "Issue";
        if (inst.op == OpType::Store) {
            entry.destination = memoryDestination(inst);
            entry.address = inst.offset + integer[inst.baseReg];
            entry.hasAddress = true;
        } else {
            entry.destination = fpReg(inst.dstFp);
        }
        entry.valueText = "";

        robTail = (robTail + 1) % config.robEntries;
        ++robCount;
        return id;
    }

    void fillStation(Station &station, const Instruction &inst, int robId, int cycle) {
        string name = station.name;
        StationKind kind = station.kind;
        station = Station{};
        station.name = name;
        station.kind = kind;
        station.busy = true;
        station.op = inst.op;
        station.instrIndex = inst.index;
        station.rob = robId;
        station.issueCycle = cycle;
        station.addressText = "";
        station.address = 0;

        if (inst.op == OpType::Load) {
            station.addressText = addressExpression(inst);
            station.address = inst.offset + integer[inst.baseReg];
            fpStatus[inst.dstFp] = RegisterTag{true, robId};
        } else if (inst.op == OpType::Store) {
            station.addressText = addressExpression(inst);
            station.address = inst.offset + integer[inst.baseReg];
            fillOperandFromFp(inst.srcFp1, station.hasVj, station.vj, station.qj);
        } else {
            fillOperandFromFp(inst.srcFp1, station.hasVj, station.vj, station.qj);
            fillOperandFromFp(inst.srcFp2, station.hasVk, station.vk, station.qk);
            fpStatus[inst.dstFp] = RegisterTag{true, robId};
        }
    }

    static void printTable(ostream &out, const vector<string> &headers, const vector<vector<string>> &rows) {
        vector<size_t> widths(headers.size(), 0);
        for (size_t i = 0; i < headers.size(); ++i) {
            widths[i] = headers[i].size();
        }
        for (const auto &row : rows) {
            for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
                widths[i] = max(widths[i], row[i].size());
            }
        }

        auto line = [&]() {
            for (size_t width : widths) {
                out << string(width + 2, '-');
            }
            out << "\n";
        };

        for (size_t i = 0; i < headers.size(); ++i) {
            out << left << setw(static_cast<int>(widths[i] + 2)) << headers[i];
        }
        out << "\n";
        line();
        for (const auto &row : rows) {
            for (size_t i = 0; i < headers.size(); ++i) {
                string cell = i < row.size() ? row[i] : "";
                out << left << setw(static_cast<int>(widths[i] + 2)) << cell;
            }
            out << "\n";
        }
    }

    void printCycle(ostream &out, int cycle) const {
        out << "\n=== Ciclo " << cycle << " ===\n";
        if (events.empty()) {
            out << "Eventos: nenhum.\n";
        } else {
            out << "Eventos:\n";
            for (const string &event : events) {
                out << "- " << event << "\n";
            }
        }
        printInstructionStatus(out);
        printRob(out);
        printReservationStations(out);
        printFpRegisterStatus(out);
    }

    static string cycleValue(int value) {
        return value < 0 ? "" : to_string(value);
    }

    static string executeRange(const InstStatus &item) {
        if (item.execStart < 0) {
            return "";
        }
        if (item.execEnd < 0) {
            return to_string(item.execStart) + "-";
        }
        if (item.execStart == item.execEnd) {
            return to_string(item.execStart);
        }
        return to_string(item.execStart) + "-" + to_string(item.execEnd);
    }

    void printInstructionStatus(ostream &out) const {
        out << "\nInstruction status\n";
        vector<vector<string>> rows;
        for (const InstStatus &item : status) {
            rows.push_back({item.text, cycleValue(item.issue), executeRange(item), cycleValue(item.writeResult), cycleValue(item.commit)});
        }
        printTable(out, {"Instruction", "Issue", "Execute", "Write result", "Commit"}, rows);
    }

    void printRob(ostream &out) const {
        out << "\nReorder buffer\n";
        vector<vector<string>> rows;
        for (const RobEntry &entry : rob) {
            rows.push_back({
                to_string(entry.id),
                entry.busy ? "Yes" : "No",
                entry.instruction,
                entry.state,
                entry.destination,
                entry.valueText
            });
        }
        printTable(out, {"Entry", "Busy", "Instruction", "State", "Destination", "Value"}, rows);
    }

    string stationValue(const Station &station, bool j) const {
        bool has = j ? station.hasVj : station.hasVk;
        double value = j ? station.vj : station.vk;
        if (has) {
            return formatDouble(value);
        }
        return "";
    }

    void printReservationStations(ostream &out) const {
        out << "\nReservation stations\n";
        vector<vector<string>> rows;
        for (const Station &station : stations) {
            rows.push_back({
                station.name,
                station.busy ? "Yes" : "No",
                station.busy ? opShort(station.op) : "",
                station.busy ? stationValue(station, true) : "",
                station.busy ? stationValue(station, false) : "",
                station.busy && station.qj != -1 ? robTag(station.qj) : "",
                station.busy && station.qk != -1 ? robTag(station.qk) : "",
                station.busy ? robTag(station.rob) : "",
                station.busy ? station.addressText : ""
            });
        }
        printTable(out, {"Name", "Busy", "Op", "Vj", "Vk", "Qj", "Qk", "Dest", "A"}, rows);
    }

    void printFpRegisterStatus(ostream &out) const {
        out << "\nFP registers status\n";
        vector<int> regs(usedFp.begin(), usedFp.end());
        if (regs.empty()) {
            regs.push_back(0);
        }

        vector<string> headers{"Field"};
        vector<string> reorder{"Reorder #"};
        vector<string> busy{"Busy"};
        for (int reg : regs) {
            headers.push_back(fpReg(reg));
            reorder.push_back(fpStatus[reg].busy ? to_string(fpStatus[reg].rob) : "");
            busy.push_back(fpStatus[reg].busy ? "Yes" : "No");
        }
        printTable(out, headers, {reorder, busy});
    }

    void printFinalRegisters(ostream &out) const {
        out << "\nValores finais dos registradores usados\n";

        vector<vector<string>> fpRows;
        for (int reg : usedFp) {
            fpRows.push_back({fpReg(reg), formatDouble(fp[reg])});
        }
        if (!fpRows.empty()) {
            out << "\nFP registers\n";
            printTable(out, {"Register", "Value"}, fpRows);
        }

        vector<vector<string>> intRows;
        for (int reg : usedInt) {
            intRows.push_back({intReg(reg), to_string(integer[reg])});
        }
        if (!intRows.empty()) {
            out << "\nInteger registers\n";
            printTable(out, {"Register", "Value"}, intRows);
        }
    }

    void printFinalMemory(ostream &out) const {
        if (usedMem.empty()) {
            return;
        }

        out << "\nMemoria acessada\n";
        vector<long long> addresses(usedMem.begin(), usedMem.end());
        sort(addresses.begin(), addresses.end());

        vector<vector<string>> rows;
        for (long long address : addresses) {
            auto it = memory.find(address);
            double value = it == memory.end() ? 0.0 : it->second;
            rows.push_back({"Mem[" + to_string(address) + "]", formatDouble(value)});
        }
        printTable(out, {"Address", "Value"}, rows);
    }

    bool printValidationReport(ostream &out) const {
        if (expectations.empty()) {
            return true;
        }

        out << "\nValidation report (EXPECT)\n";
        vector<vector<string>> rows;
        bool allPassed = true;

        for (const Expectation &expectation : expectations) {
            double actual = 0.0;
            string actualText;
            string expectedText;
            bool passed = false;

            switch (expectation.kind) {
            case ExpectKind::FpRegister:
                actual = fp[expectation.index];
                actualText = formatDouble(actual);
                expectedText = formatDouble(expectation.number);
                passed = fabs(actual - expectation.number) < 1e-9;
                break;
            case ExpectKind::IntRegister:
                actual = static_cast<double>(integer[expectation.index]);
                actualText = to_string(integer[expectation.index]);
                expectedText = to_string(static_cast<long long>(expectation.number));
                passed = fabs(actual - expectation.number) < 1e-9;
                break;
            case ExpectKind::Memory: {
                auto it = memory.find(expectation.address);
                actual = it == memory.end() ? 0.0 : it->second;
                actualText = formatDouble(actual);
                expectedText = formatDouble(expectation.number);
                passed = fabs(actual - expectation.number) < 1e-9;
                break;
            }
            case ExpectKind::Cycles:
                actual = static_cast<double>(finalCycles);
                actualText = to_string(finalCycles);
                expectedText = to_string(static_cast<int>(expectation.number));
                passed = fabs(actual - expectation.number) < 1e-9;
                break;
            case ExpectKind::InstructionIssue:
                actual = static_cast<double>(status[expectation.index].issue);
                actualText = cycleValue(status[expectation.index].issue);
                expectedText = to_string(static_cast<int>(expectation.number));
                passed = status[expectation.index].issue == static_cast<int>(expectation.number);
                break;
            case ExpectKind::InstructionExecute:
                actualText = executeRange(status[expectation.index]);
                expectedText = static_cast<int>(expectation.number) == static_cast<int>(expectation.secondNumber)
                                   ? to_string(static_cast<int>(expectation.number))
                                   : to_string(static_cast<int>(expectation.number)) + "-" +
                                         to_string(static_cast<int>(expectation.secondNumber));
                passed = status[expectation.index].execStart == static_cast<int>(expectation.number) &&
                         status[expectation.index].execEnd == static_cast<int>(expectation.secondNumber);
                break;
            case ExpectKind::InstructionWrite:
                actual = static_cast<double>(status[expectation.index].writeResult);
                actualText = cycleValue(status[expectation.index].writeResult);
                expectedText = to_string(static_cast<int>(expectation.number));
                passed = status[expectation.index].writeResult == static_cast<int>(expectation.number);
                break;
            case ExpectKind::InstructionCommit:
                actual = static_cast<double>(status[expectation.index].commit);
                actualText = cycleValue(status[expectation.index].commit);
                expectedText = to_string(static_cast<int>(expectation.number));
                passed = status[expectation.index].commit == static_cast<int>(expectation.number);
                break;
            }

            allPassed = allPassed && passed;
            rows.push_back({
                expectation.label,
                expectedText,
                actualText,
                passed ? "OK" : "FAIL",
                to_string(expectation.lineNo)
            });
        }

        printTable(out, {"Check", "Expected", "Actual", "Status", "Line"}, rows);
        out << (allPassed ? "Resultado: todos os EXPECT passaram.\n" : "Resultado: ha EXPECT com falha.\n");
        return allPassed;
    }
};

static void printUsage(const char *programName) {
    cout << "Uso: " << programName << " <entrada.txt> [--step] [--quiet] [--max-cycles N]\n"
         << "\n"
         << "  --step          pausa a cada ciclo, permitindo acompanhar ciclo a ciclo\n"
         << "  --quiet         mostra apenas o resumo final\n"
         << "  --max-cycles N  evita loop infinito em entradas invalidas\n";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    string inputPath = argv[1];
    bool trace = true;
    bool step = false;
    int maxCycles = 100000;

    for (int i = 2; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--step") {
            step = true;
        } else if (arg == "--quiet") {
            trace = false;
        } else if (arg == "--max-cycles") {
            if (i + 1 >= argc) {
                cerr << "--max-cycles exige um valor.\n";
                return 1;
            }
            maxCycles = atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            cerr << "Opcao desconhecida: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    try {
        ProgramInput input = parseInputFile(inputPath);
        TomasuloSimulator simulator(std::move(input));
        bool ok = simulator.run(trace, step, maxCycles);
        if (!ok) {
            return 2;
        }
    } catch (const exception &ex) {
        cerr << "Erro: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
