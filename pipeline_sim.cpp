// pipeline_sim.cpp - 5-stage (IF ID EX MEM WB) RISC pipeline simulator
// Models: data hazards (stall or forwarding), load-use stall, branch prediction (static / 2-bit).
// Every run is checked against a simple one-instruction-at-a-time reference model.
#include <bits/stdc++.h>
using namespace std;

enum Op { ADD, SUB, AND, OR, SLT, ADDI, LW, SW, BEQ, BNE, NOP, HALT };

struct Instr { Op op = NOP; int rd = 0, rs1 = 0, rs2 = 0, imm = 0, target = 0; };

static bool writesReg(const Instr& i) {
    switch (i.op) { case ADD: case SUB: case AND: case OR: case SLT: case ADDI: case LW: return i.rd != 0; default: return false; }
}
static bool usesRs1(const Instr& i) { return i.op != NOP && i.op != HALT; }
static bool usesRs2(const Instr& i) {
    switch (i.op) { case ADD: case SUB: case AND: case OR: case SLT: case SW: case BEQ: case BNE: return true; default: return false; }
}
static bool isBranch(const Instr& i) { return i.op == BEQ || i.op == BNE; }

static string fmt(const Instr& i) {
    static const char* n[] = {"add","sub","and","or","slt","addi","lw","sw","beq","bne","nop","halt"};
    ostringstream o; o << n[i.op];
    switch (i.op) {
        case ADD: case SUB: case AND: case OR: case SLT: o << " x" << i.rd << ",x" << i.rs1 << ",x" << i.rs2; break;
        case ADDI: o << " x" << i.rd << ",x" << i.rs1 << "," << i.imm; break;
        case LW: o << " x" << i.rd << "," << i.imm << "(x" << i.rs1 << ")"; break;
        case SW: o << " x" << i.rs2 << "," << i.imm << "(x" << i.rs1 << ")"; break;
        case BEQ: case BNE: o << " x" << i.rs1 << ",x" << i.rs2 << ",@" << i.target; break;
        default: break;
    }
    return o.str();
}

// ------------------------------------------------------------------ assembler
static int parseReg(const string& t) {
    if (t.size() < 2 || t[0] != 'x') throw runtime_error("bad register: " + t);
    int r = stoi(t.substr(1));
    if (r < 0 || r > 31) throw runtime_error("register out of range: " + t);
    return r;
}

static vector<Instr> parseProgram(istream& in) {
    vector<vector<string>> lines; map<string, int> labels; string line;
    while (getline(in, line)) {
        size_t p = line.find_first_of("#;"); if (p != string::npos) line.resize(p);
        for (char& c : line) if (c == ',' || c == '(' || c == ')') c = ' ';
        stringstream ss(line); vector<string> tok; string t;
        while (ss >> t) tok.push_back(t);
        while (!tok.empty() && tok[0].back() == ':') {
            labels[tok[0].substr(0, tok[0].size() - 1)] = lines.size(); tok.erase(tok.begin());
        }
        if (!tok.empty()) { for (char& c : tok[0]) c = tolower(c); lines.push_back(tok); }
    }
    vector<Instr> prog;
    for (auto& tk : lines) {
        Instr i; const string& m = tk[0];
        auto need = [&](size_t n) { if (tk.size() != n) throw runtime_error("wrong operand count: " + m); };
        if (m == "add" || m == "sub" || m == "and" || m == "or" || m == "slt") {
            need(4); i.op = m == "add" ? ADD : m == "sub" ? SUB : m == "and" ? AND : m == "or" ? OR : SLT;
            i.rd = parseReg(tk[1]); i.rs1 = parseReg(tk[2]); i.rs2 = parseReg(tk[3]);
        } else if (m == "addi") { need(4); i.op = ADDI; i.rd = parseReg(tk[1]); i.rs1 = parseReg(tk[2]); i.imm = stoi(tk[3]); }
        else if (m == "lw")     { need(4); i.op = LW; i.rd = parseReg(tk[1]); i.imm = stoi(tk[2]); i.rs1 = parseReg(tk[3]); }
        else if (m == "sw")     { need(4); i.op = SW; i.rs2 = parseReg(tk[1]); i.imm = stoi(tk[2]); i.rs1 = parseReg(tk[3]); }
        else if (m == "beq" || m == "bne") {
            need(4); i.op = m == "beq" ? BEQ : BNE; i.rs1 = parseReg(tk[1]); i.rs2 = parseReg(tk[2]);
            if (!labels.count(tk[3])) throw runtime_error("unknown label: " + tk[3]);
            i.target = labels[tk[3]];
        } else if (m == "nop") { i.op = NOP; }
        else if (m == "halt") { i.op = HALT; }
        else throw runtime_error("unknown instruction: " + m);
        prog.push_back(i);
    }
    if (prog.empty() || prog.back().op != HALT) { Instr h; h.op = HALT; prog.push_back(h); }
    return prog;
}

// ------------------------------------------------------------------ machine state + shared ALU
const int MEM_WORDS = 1024;
struct State { array<int, 32> regs{}; vector<int> mem = vector<int>(MEM_WORDS, 0); };

static int memIdx(int addr) {
    if (addr < 0 || addr % 4 != 0 || addr / 4 >= MEM_WORDS) throw runtime_error("bad memory address " + to_string(addr));
    return addr / 4;
}
static int alu(const Instr& i, int a, int b) {
    switch (i.op) {
        case ADD: return a + b; case SUB: return a - b; case AND: return a & b; case OR: return a | b;
        case SLT: return a < b; case ADDI: case LW: case SW: return a + i.imm;
        default: return 0;
    }
}
static bool branchTaken(const Instr& i, int a, int b) { return i.op == BEQ ? a == b : a != b; }

// ------------------------------------------------------------------ reference model (no pipeline)
static long long runReference(const vector<Instr>& prog, State& s) {
    int pc = 0; long long n = 0;
    while (true) {
        if (pc < 0 || pc >= (int)prog.size()) throw runtime_error("pc out of range");
        const Instr& i = prog[pc];
        if (i.op == HALT) break;
        if (++n > 20000000) throw runtime_error("program does not terminate");
        int a = s.regs[i.rs1], b = s.regs[i.rs2], next = pc + 1;
        if (i.op == LW) { if (i.rd) s.regs[i.rd] = s.mem[memIdx(a + i.imm)]; }
        else if (i.op == SW) s.mem[memIdx(a + i.imm)] = b;
        else if (isBranch(i)) { if (branchTaken(i, a, b)) next = i.target; }
        else if (writesReg(i)) s.regs[i.rd] = alu(i, a, b);
        pc = next;
    }
    return n;
}

// ------------------------------------------------------------------ the pipeline
struct Slot {                       // one pipeline register (holds one instruction)
    bool valid = false; Instr in; int pc = 0;
    int a = 0, b = 0;               // operand values (b is also the store data for SW)
    int result = 0;                 // ALU result, later the loaded value
    bool predTaken = false;
};
struct Config { bool fwd = true; string pred = "nt"; bool trace = false; bool injectBug = false; };
struct Stats { long long cycles = 0, instrs = 0, dataStalls = 0, branches = 0, mispredicts = 0; };

static string slotName(const Slot& s) { return s.valid ? fmt(s.in) : "-"; }

static Stats runPipeline(const vector<Instr>& prog, State& s, const Config& cfg) {
    Slot if_id, id_ex, ex_mem, mem_wb;
    vector<int> bht(64, 1);         // 2-bit counters: 0,1 = not taken, 2,3 = taken
    int pc = 0; bool fetchStopped = false, done = false;
    Stats st;
    auto predict = [&](int p) { return cfg.pred == "nt" ? false : cfg.pred == "taken" ? true : bht[p % 64] >= 2; };

    while (!done) {
        if (++st.cycles > 50000000) throw runtime_error("pipeline did not finish");
        Slot n_ifid, n_idex, n_exmem, n_memwb;
        string fetched = "-";

        // ---- WB: write register file (happens before ID reads, in the same cycle)
        if (mem_wb.valid) {
            if (writesReg(mem_wb.in)) s.regs[mem_wb.in.rd] = mem_wb.result;
            if (mem_wb.in.op == HALT) done = true; else st.instrs++;
        }
        // ---- MEM
        if (ex_mem.valid) {
            n_memwb = ex_mem;
            if (ex_mem.in.op == LW) n_memwb.result = s.mem[memIdx(ex_mem.result)];
            if (ex_mem.in.op == SW) s.mem[memIdx(ex_mem.result)] = ex_mem.b;
        }
        // ---- EX (uses the OLD ex_mem / mem_wb for forwarding)
        bool redirect = false; int newpc = 0;
        if (id_ex.valid) {
            Slot cur = id_ex; int a = cur.a, b = cur.b;
            if (cfg.fwd) {
                auto fwd = [&](int r, int v) {
                    if (r == 0) return v;
                    if (ex_mem.valid && writesReg(ex_mem.in) && ex_mem.in.rd == r && ex_mem.in.op != LW) return ex_mem.result; // from EX/MEM
                    if (mem_wb.valid && writesReg(mem_wb.in) && mem_wb.in.rd == r) return mem_wb.result;                      // from MEM/WB
                    return v;
                };
                if (usesRs1(cur.in)) a = fwd(cur.in.rs1, a);
                if (usesRs2(cur.in)) b = fwd(cur.in.rs2, b);
            }
            cur.a = a; cur.b = b; cur.result = alu(cur.in, a, b);
            if (isBranch(cur.in)) {
                bool taken = branchTaken(cur.in, a, b);
                st.branches++;
                int& c = bht[cur.pc % 64];
                c = taken ? min(3, c + 1) : max(0, c - 1);
                if (taken != cur.predTaken) {            // misprediction: flush the 2 younger instructions
                    st.mispredicts++; redirect = true; newpc = taken ? cur.in.target : cur.pc + 1;
                }
            }
            n_exmem = cur;
        }
        // ---- ID + IF
        bool stall = false;
        if (redirect) {
            pc = newpc; fetchStopped = false; fetched = "FLUSH";   // n_ifid, n_idex stay bubbles
        } else {
            if (if_id.valid) {
                const Instr& i = if_id.in;
                auto conflict = [&](const Slot& p) {
                    return p.valid && writesReg(p.in) &&
                           ((usesRs1(i) && i.rs1 == p.in.rd) || (usesRs2(i) && i.rs2 == p.in.rd));
                };
                if (cfg.fwd) stall = !cfg.injectBug && id_ex.valid && id_ex.in.op == LW && conflict(id_ex); // load-use
                else         stall = conflict(id_ex) || conflict(ex_mem);                                    // no forwarding
                if (!stall) { n_idex = if_id; n_idex.a = s.regs[i.rs1]; n_idex.b = s.regs[i.rs2]; }
                else st.dataStalls++;
            }
            if (stall) { n_ifid = if_id; fetched = "(stall)"; }       // hold IF/ID and PC
            else if (!fetchStopped && pc >= 0 && pc < (int)prog.size()) {
                Slot f; f.valid = true; f.in = prog[pc]; f.pc = pc; int next = pc + 1;
                if (isBranch(f.in)) { f.predTaken = predict(pc); if (f.predTaken) next = f.in.target; }
                if (f.in.op == HALT) fetchStopped = true;
                n_ifid = f; fetched = fmt(f.in); pc = next;
            }
        }
        if (cfg.trace)
            printf("%4lld | IF %-16s| ID %-16s| EX %-16s| MEM %-16s| WB %-16s\n", st.cycles, fetched.c_str(),
                   slotName(if_id).c_str(), slotName(id_ex).c_str(), slotName(ex_mem).c_str(), slotName(mem_wb).c_str());
        if_id = n_ifid; id_ex = n_idex; ex_mem = n_exmem; mem_wb = n_memwb;
    }
    return st;
}

// ------------------------------------------------------------------ driver
static bool sameState(const State& x, const State& y) { return x.regs == y.regs && x.mem == y.mem; }

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "usage: pipeline_sim prog.asm [--no-forward] [--predictor nt|taken|2bit] [--trace] [--sweep] [--quiet] [--inject-bug]\n";
        return 1;
    }
    Config cfg; bool sweep = false, quiet = false;
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        if (a == "--no-forward") cfg.fwd = false;
        else if (a == "--predictor" && i + 1 < argc) cfg.pred = argv[++i];
        else if (a == "--trace") cfg.trace = true;
        else if (a == "--sweep") sweep = true;
        else if (a == "--quiet") quiet = true;
        else if (a == "--inject-bug") cfg.injectBug = true;
        else { cerr << "unknown option: " << a << "\n"; return 1; }
    }
    try {
        ifstream f(argv[1]);
        if (!f) { cerr << "cannot open " << argv[1] << "\n"; return 1; }
        vector<Instr> prog = parseProgram(f);
        State ref; long long refN = runReference(prog, ref);

        if (sweep) {
            if (!quiet) printf("%-11s %-10s %8s %8s %6s %11s %12s  %s\n", "forwarding", "predictor", "cycles", "instrs", "CPI", "data-stalls", "mispredicts", "check");
            bool allOk = true;
            for (bool fw : {false, true}) for (string pr : {"nt", "taken", "2bit"}) {
                Config c = cfg; c.fwd = fw; c.pred = pr; c.trace = false;
                State s; Stats st = runPipeline(prog, s, c);
                bool ok = sameState(s, ref) && st.instrs == refN; allOk &= ok;
                if (!quiet) printf("%-11s %-10s %8lld %8lld %6.2f %11lld %12lld  %s\n", fw ? "on" : "off", pr.c_str(),
                                   st.cycles, st.instrs, (double)st.cycles / st.instrs, st.dataStalls, st.mispredicts, ok ? "PASS" : "FAIL");
            }
            if (!quiet) printf(allOk ? "\nAll configurations match the reference model.\n" : "\nMISMATCH vs reference model!\n");
            return allOk ? 0 : 2;
        }
        State s; Stats st = runPipeline(prog, s, cfg);
        bool ok = sameState(s, ref) && st.instrs == refN;
        printf("\nforwarding=%s predictor=%s\n", cfg.fwd ? "on" : "off", cfg.pred.c_str());
        printf("cycles=%lld instructions=%lld CPI=%.2f data-stall-cycles=%lld branches=%lld mispredicts=%lld\n",
               st.cycles, st.instrs, (double)st.cycles / st.instrs, st.dataStalls, st.branches, st.mispredicts);
        printf("registers:"); for (int r = 1; r < 32; r++) if (s.regs[r]) printf(" x%d=%d", r, s.regs[r]);
        printf("\nmemory   :"); int shown = 0;
        for (int w = 0; w < MEM_WORDS && shown < 16; w++) if (s.mem[w]) { printf(" [%d]=%d", 4 * w, s.mem[w]); shown++; }
        printf("\nreference model check: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 2;
    } catch (exception& e) { cerr << "error: " << e.what() << "\n"; return 1; }
}
