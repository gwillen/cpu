#include <stdlib.h>

#include <boost/optional/optional.hpp>
#include <boost/format.hpp>

// Set bit a through bit b (inclusive), as long as 0 <= a <= 31 and 0 <= b <= 31.
// From http://stackoverflow.com/a/8774613 .
#define BIT_MASK(a, b) (((unsigned) -1 >> (31 - (b))) & ~((1U << (a)) - 1))

#define BITS(word, idx, count) ((word & BIT_MASK(idx, idx + count - 1)) >> idx)
#define BIT(word, idx) BITS(word, idx, 1)

#define array_size(x) (sizeof(x) / sizeof((x)[0]))

typedef uint32_t address_t;
typedef uint32_t data_t;
typedef char lane_t;  // -1 for 'N/A' (i.e. for hardware interrupt events)

struct event {
    lane_t lane;
};

struct mem_event : public event {
    address_t addr;
};

struct mem_read_event : public mem_event {};

struct mem_read_result : public mem_event {
    boost::optional<data_t> data;
};

typedef void (instr_bottom_half)(mem_read_result);

struct mem_write_event : public mem_event {
    data_t data;
    unsigned int byte_enable : 4;
};

struct instruction_continuation {
    mem_read_event query;
    instr_bottom_half k;
};

typedef boost::optional<instruction_continuation> instruction_result;

struct instruction_commit {
    // XXX either up to four registers to commit, OR an exception or interrupt. 
    // XXX don't forget about other things we might need to commit, such as:
    // - link flag
    // - extra-maths register (for div and mul)
    // - PC, in case of a branch
};

typedef uint32_t instruction;
typedef instruction instruction_packet[4];

enum optype_t {
    BRANCH_OP,
    ALU_OP,
    LSU_OP,
    OTHER_OP,
    INVALID_OP
};

const size_t OPTYPES_COUNT = INVALID_OP + 1;
const size_t MAX_OPCODE_LEN = 16;

const char OPTYPE_STR[][MAX_OPCODE_LEN] = {
    "<BRANCH_OP>",
    "<ALU_OP>",
    "<LSU_OP>",
    "<OTHER_OP>",
    "<INVALID_OP>"
};
static_assert(array_size(OPTYPE_STR) == OPTYPES_COUNT, "Optype count mismatch");


// Must match instruction encoding
enum aluop_t {
    ALU_ADD,
    ALU_AND,
    ALU_NOR,
    ALU_OR,
    ALU_SUB,
    ALU_RSB,
    ALU_XOR,
    ALU_COMPARE,
    ALU_MOV,
    ALU_MVN,
    ALU_SXB,
    ALU_SXH,
    ALU_RESV1,
    ALU_RESV2,
    ALU_RESV3,
    ALU_RESV4
};

const size_t ALUOPS_COUNT = ALU_RESV4 + 1;
static_assert(ALUOPS_COUNT == 16, "Bad alu op list");

const char ALUOP_STR[][MAX_OPCODE_LEN] = {
    "ADD",
    "AND",
    "NOR",
    "OR",
    "SUB",
    "RSB",
    "XOR",
    "<COMPARE>",
    "MOV",
    "MVN",
    "SXB",
    "SXH",
    "<RESERVED 1>",
    "<RESERVED 2>",
    "<RESERVED 3>",
    "<RESERVED 4>"
};
static_assert(array_size(ALUOP_STR) == ALUOPS_COUNT, "Aluops count mismatch");


// Must match instruction encoding
enum cmpop_t {
    CMP_LTU,
    CMP_LEU,
    CMP_EQ,
    CMP_RESV,
    CMP_LTS,
    CMP_LES,
    CMP_BS,
    CMP_BC
};

const size_t CMPOPS_COUNT = CMP_BC + 1;
static_assert(CMPOPS_COUNT == 8, "Bad cmp op list");

const char CMPOP_STR[][MAX_OPCODE_LEN] = {
    "LTU",
    "LEU",
    "EQ",
    "<RESERVED>",
    "LTS",
    "LES",
    "BS",
    "BC"
};
static_assert(array_size(CMPOP_STR) == CMPOPS_COUNT, "Cmpops count mismatch");


// Order must match instruction encoding
enum shift_type {
    LSL,
    LSR,
    ASR,
    ROR
};


// Order must match instruction encoding
enum lsuop_t {
    LS_LB,
    LS_LHW,
    LS_LW,
    LS_LL,
    LS_SB,
    LS_SHW,
    LS_SW,
    LS_SC
};

const size_t LSUOPS_COUNT = LS_SC + 1;
static_assert(LSUOPS_COUNT == 8, "Bad lsu op list");

const char LSUOP_STR[][MAX_OPCODE_LEN] = {
    "LB",
    "LHW",
    "LW",
    "LL",
    "SB",
    "SHW",
    "SW",
    "SC"
};
static_assert(array_size(LSUOP_STR) == LSUOPS_COUNT, "Lsuops count mismatch");


// Order must match instruction encoding
enum otherop_t {
    OTHER_RESV0,
    OTHER_BREAK,
    OTHER_SYSCALL,
    OTHER_FENCE,
    OTHER_ERET,
    OTHER_CPOP,
    OTHER_MVC,
    OTHER_MTC,
    OTHER_MULT,
    OTHER_DIV,
    OTHER_MFHI,
    OTHER_MTHI,
    OTHER_SIMD0,
    OTHER_SIMD1,
    OTHER_SIMD2,
    OTHER_SIMD3
};

const size_t OTHEROPS_COUNT = OTHER_SIMD3 + 1;
static_assert(OTHEROPS_COUNT == 16, "Bad other op list");

const char OTHEROP_STR[][MAX_OPCODE_LEN] = {
    "<RESV0>",
    "BREAK",
    "SYSCALL",
    "FENCE",
    "ERET",
    "CPOP",
    "MFC",
    "MTC",
    "MULT",
    "DIV",
    "MFHI",
    "MTHI",
    "<SIMD0>",
    "<SIMD1>",
    "<SIMD2>",
    "<SIMD3>"
};
static_assert(array_size(OTHEROP_STR) == OTHEROPS_COUNT, "Otherops count mismatch");


struct reg_t {
    unsigned int reg:5;

    reg_t (unsigned int r) : reg(r) {}
    operator int() { return reg; }
};

struct pred_reg_t {
    unsigned int reg:2;

    pred_reg_t (unsigned int r) : reg(r) {}
    operator int() { return reg; }
};

std::string string_format(const std::string fmt_str, ...);

struct regs_t {
    uint32_t r[32];
    uint32_t pc;
};

struct cpu_t {
    regs_t regs;
};

struct decoded_instruction {
    uint32_t raw_instr;  // For debugging

    pred_reg_t pred_reg = 0;
    bool pred_comp;
    optype_t optype = INVALID_OP;
    boost::optional<bool> branch_link;
    boost::optional<aluop_t> aluop;
    boost::optional<cmpop_t> cmpop;
    boost::optional<lsuop_t> lsuop;
    boost::optional<otherop_t> otherop;
    boost::optional<uint32_t> constant;
    boost::optional<int32_t> offset;
    boost::optional<reg_t> rs;
    boost::optional<reg_t> rd;
    boost::optional<reg_t> rt;
    boost::optional<pred_reg_t> pd;
    boost::optional<uint32_t> shiftamt;
    boost::optional<uint32_t> stype;
    boost::optional<uint32_t> reserved_bits;
    bool long_imm = false;

    decoded_instruction(instruction);

    decoded_instruction() {
        // XXX: I'm so sorry.
    }

    bool alu_unary();
    bool alu_binary();
    bool alu_compare();

    std::string branchop_str();
    std::string opcode_str();
    std::string to_string();

    bool execute_instruction(cpu_t &cpu, uint32_t old_pc);
};

struct decoded_packet {
    decoded_instruction instr[4];

    decoded_packet(instruction_packet);

    std::string to_string();

    bool execute_packet(cpu_t &cpu);
};

uint32_t shiftwith(uint32_t value, uint32_t shiftamt, shift_type stype);
uint32_t rand32();