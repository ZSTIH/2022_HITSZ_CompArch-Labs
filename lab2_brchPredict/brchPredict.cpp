#include <iostream>
#include <fstream>
#include <cassert>
#include <stdarg.h>
#include <cstdlib>
#include <cstring>
#include "pin.H"

using namespace std;

typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long int   UINT64;
typedef unsigned __int128   UINT128;



ofstream OutFile;

// ��val�ض�, ʹ���ȱ��bits
#define truncate(val, bits) ((val) & ((1 << (bits)) - 1))

static UINT64 takenCorrect = 0;
static UINT64 takenIncorrect = 0;
static UINT64 notTakenCorrect = 0;
static UINT64 notTakenIncorrect = 0;

// ���ͼ����� (N < 64)
class SaturatingCnt
{
    size_t m_wid;
    UINT8 m_val;
    const UINT8 m_init_val;

    public:
        SaturatingCnt(size_t width = 2) : m_init_val((1 << width) / 2)
        {
            m_wid = width;
            m_val = m_init_val;
        }

        void increase() { if (m_val < (1 << m_wid) - 1) m_val++; }
        void decrease() { if (m_val > 0) m_val--; }

        void reset() { m_val = m_init_val; }
        UINT8 getVal() { return m_val; }

        bool isTaken() { return (m_val > (1 << m_wid)/2 - 1); }
};

// ��λ�Ĵ��� (N < 128)
class ShiftReg
{
    size_t m_wid;
    UINT128 m_val;

    public:
        ShiftReg(size_t width) : m_wid(width), m_val(0) {}

        bool shiftIn(bool b)
        {
            bool ret = !!(m_val & (1 << (m_wid - 1)));
            m_val <<= 1;
            m_val |= b;
            m_val &= (1 << m_wid) - 1;
            return ret;
        }

        UINT128 getVal() { return m_val; }
};

// Hash functions
inline UINT128 f_xor(UINT128 a, UINT128 b) { return a ^ b; }
inline UINT128 f_xor1(UINT128 a, UINT128 b) { return ~a ^ ~b; }
inline UINT128 f_xnor(UINT128 a, UINT128 b) { return ~(a ^ ~b); }



// Base class of all predictors
class BranchPredictor
{
    public:
        BranchPredictor() {}
        virtual ~BranchPredictor() {}
        virtual bool predict(ADDRINT addr) { return false; };
        virtual void update(bool takenActually, bool takenPredicted, ADDRINT addr) {};
};

BranchPredictor* BP;



/* ===================================================================== */
/* BHT-based branch predictor                                            */
/* ===================================================================== */
class BHTPredictor: public BranchPredictor
{
    size_t m_entries_log;
    SaturatingCnt* m_scnt;              // BHT
    allocator<SaturatingCnt> m_alloc;
    
    public:
        // Constructor
        // param:   entry_num_log:  BHT�����Ķ���
        //          scnt_width:     ���ͼ�������λ��, Ĭ��ֵΪ2
        BHTPredictor(size_t entry_num_log, size_t scnt_width = 2)
        {
            m_entries_log = entry_num_log;

            m_scnt = m_alloc.allocate(1 << entry_num_log);      // Allocate memory for BHT
            for (int i = 0; i < (1 << entry_num_log); i++)
                m_alloc.construct(m_scnt + i, scnt_width);      // Call constructor of SaturatingCnt
        }

        // Destructor
        ~BHTPredictor()
        {
            for (int i = 0; i < (1 << m_entries_log); i++)
                m_alloc.destroy(m_scnt + i);

            m_alloc.deallocate(m_scnt, 1 << m_entries_log);
        }

        BOOL predict(ADDRINT addr)
        {
            // TODO: Produce prediction according to BHT
            return m_scnt[truncate(addr, m_entries_log)].isTaken();
        }

        void update(BOOL takenActually, BOOL takenPredicted, ADDRINT addr)
        {
            // TODO: Update BHT according to branch results and prediction
            if (takenActually) {
                m_scnt[truncate(addr, m_entries_log)].increase();
            } else {
                m_scnt[truncate(addr, m_entries_log)].decrease();
            }
        }
};

/* ===================================================================== */
/* Global-history-based branch predictor                                 */
/* ===================================================================== */
template<UINT128 (*hash)(UINT128 addr, UINT128 history)>
class GlobalHistoryPredictor: public BranchPredictor
{
    ShiftReg* m_ghr;                   // GHR
    SaturatingCnt* m_scnt;              // PHT�еķ�֧��ʷ�ֶ�
    size_t m_entries_log;                   // PHT�����Ķ���
    allocator<SaturatingCnt> m_alloc;
    
    public:
        // Constructor
        // param:   ghr_width:      Width of GHR
        //          entry_num_log:  PHT�������Ķ���
        //          scnt_width:     ���ͼ�������λ��, Ĭ��ֵΪ2
        GlobalHistoryPredictor(size_t ghr_width, size_t entry_num_log, size_t scnt_width = 2)
        {
            // TODO:
            m_ghr = new ShiftReg(ghr_width);
            m_entries_log = entry_num_log;

            m_scnt = m_alloc.allocate(1 << entry_num_log);      // Allocate memory for PHT
            for (int i = 0; i < (1 << entry_num_log); i++)
                m_alloc.construct(m_scnt + i, scnt_width);      // Call constructor of SaturatingCnt
        }

        // Destructor
        ~GlobalHistoryPredictor()
        {
            // TODO
            for (int i = 0; i < (1 << m_entries_log); i++)
                m_alloc.destroy(m_scnt + i);

            m_alloc.deallocate(m_scnt, 1 << m_entries_log);
        }

        // Only for TAGE: return a tag according to the specificed address
        UINT128 get_tag(ADDRINT addr)
        {
            // TODO
            UINT128 hash_result = hash(addr, get_ghr());
            return truncate(hash_result, m_entries_log);
        }

        // Only for TAGE: return GHR's value
        UINT128 get_ghr()
        {
            // TODO
            return m_ghr->getVal();
        }

        // Only for TAGE: reset a saturating counter to default value (which is weak taken)
        void reset_ctr(ADDRINT addr)
        {
            // TODO
            m_scnt[get_tag(addr)].reset();
        }

        bool predict(ADDRINT addr)
        {
            // TODO: Produce prediction according to GHR and PHT
            return m_scnt[get_tag(addr)].isTaken();
        }

        void update(bool takenActually, bool takenPredicted, ADDRINT addr)
        {
            // TODO: Update GHR and PHT according to branch results and prediction
            if (takenActually) {
                m_scnt[get_tag(addr)].increase();
            } else {
                m_scnt[get_tag(addr)].decrease();
            }
            if (takenActually) {
                m_ghr->shiftIn(1);
            } else {
                m_ghr->shiftIn(0);
            }
        }
};

/* ===================================================================== */
/* Tournament predictor: Select output by global/local selection history */
/* ===================================================================== */
class TournamentPredictor: public BranchPredictor
{
    BranchPredictor* m_BPs[2];      // Sub-predictors
    SaturatingCnt* m_gshr;          // Global select-history register

    public:
        TournamentPredictor(BranchPredictor* BP0, BranchPredictor* BP1, size_t gshr_width = 2)
        {
            // TODO
            m_BPs[0] = BP0;
            m_BPs[1] = BP1;
            m_gshr = new SaturatingCnt(gshr_width);
        }

        ~TournamentPredictor()
        {
            // TODO
            delete m_gshr;
            delete m_BPs[0];
            delete m_BPs[1];
        }

        // TODO

        BOOL predict(ADDRINT addr)
        {
            if (m_gshr->isTaken()) {
                return m_BPs[1]->predict(addr);
            } else {
                return m_BPs[0]->predict(addr);
            }
        }

        void update(BOOL takenActually, BOOL takenPredicted, ADDRINT addr)
        {
            m_BPs[0]->update(takenActually, takenPredicted, addr);
            m_BPs[1]->update(takenActually, takenPredicted, addr);
            bool result0 = m_BPs[0]->predict(addr);
            bool result1 = m_BPs[1]->predict(addr);
            if (result0 == result1) {
                // ����Ԥ����Ԥ������ͬ
                return;
            } else {
                if (result0 == takenActually) {
                    // ����Ԥ����0Ԥ����ȷ
                    m_gshr->decrease();
                } else {
                    // ����Ԥ����1Ԥ����ȷ
                    m_gshr->increase();
                }
            }
        }

};

/* ===================================================================== */
/* TArget GEometric history length Predictor                             */
/* ===================================================================== */
template<UINT128 (*hash1)(UINT128 pc, UINT128 ghr), UINT128 (*hash2)(UINT128 pc, UINT128 ghr)>
class TAGEPredictor: public BranchPredictor
{
    const size_t m_tnum;            // ��Ԥ�������� (T[0 : m_tnum - 1])
    const size_t m_entries_log;     // ��Ԥ����T[1 : m_tnum - 1]��PHT�����Ķ���
    BranchPredictor** m_T;          // ��Ԥ����ָ������
    bool* m_T_pred;                 // ���ڴ洢����Ԥ���Ԥ��ֵ
    UINT8** m_useful;               // usefulness matrix
    UINT128** m_tag;                // tag matrix
    int m_tag_width;                // width of tag
    int provider_indx;              // Provider's index of m_T
    int altpred_indx;               // Alternate provider's index of m_T

    const size_t m_rst_period;      // Reset period of usefulness
    size_t m_rst_cnt;               // Reset counter

    public:
        // Constructor
        // param:   tnum:               The number of sub-predictors
        //          T0_entry_num_log:   ��Ԥ����T0��BHT�����Ķ���
        //          T1ghr_len:          ��Ԥ����T1��GHRλ��
        //          alpha:              ����Ԥ����T[1 : m_tnum - 1]��GHR���α�����ϵ
        //          Tn_entry_num_log:   ����Ԥ����T[1 : m_tnum - 1]��PHT�����Ķ���
        //          scnt_width:         Width of saturating counter (3 by default)
        //          rst_period:         Reset period of usefulness
        TAGEPredictor(size_t tnum, size_t T0_entry_num_log, size_t T1ghr_len, float alpha, size_t Tn_entry_num_log, size_t scnt_width = 3, int tag_width = 3, size_t rst_period = 256*1024)
        : m_tnum(tnum), m_entries_log(Tn_entry_num_log), m_tag_width(tag_width), m_rst_period(rst_period), m_rst_cnt(0)
        {
            m_T = new BranchPredictor* [m_tnum];
            m_T_pred = new bool [m_tnum];
            m_useful = new UINT8* [m_tnum];
            m_tag = new UINT128* [m_tnum];

            m_T[0] = new BHTPredictor(T0_entry_num_log);

            size_t ghr_size = T1ghr_len;
            for (size_t i = 1; i < m_tnum; i++)
            {
                m_T[i] = new GlobalHistoryPredictor<hash1>(ghr_size, m_entries_log, scnt_width);
                ghr_size = (size_t)(ghr_size * alpha);

                m_useful[i] = new UINT8 [1 << m_entries_log];
                m_tag[i] = new UINT128 [1 << m_entries_log];
                memset(m_useful[i], 0, sizeof(UINT8)*(1 << m_entries_log));
                memset(m_tag[i], 0, sizeof(UINT128)*(1 << m_entries_log));
            }
        }

        ~TAGEPredictor()
        {
            for (size_t i = 0; i < m_tnum; i++) delete m_T[i];
            for (size_t i = 0; i < m_tnum; i++) delete[] m_useful[i];

            delete[] m_T;
            delete[] m_T_pred;
            delete[] m_useful;
        }

        bool predict(ADDRINT addr)
        {
            // TODO

            for (size_t i = 0; i < m_tnum; i++) {
                m_T_pred[i] = m_T[i]->predict(addr);
            }

            provider_indx = 0;
            altpred_indx = 0;

            for (size_t i = 1; i < m_tnum; i++) {
                GlobalHistoryPredictor<hash1>* ghp = (GlobalHistoryPredictor<hash1>*) m_T[i];
                UINT128 h2 = hash2(addr, ghp->get_ghr());
                UINT128 tag = m_tag[i][ghp->get_tag(addr)];
                h2 = truncate(h2, m_tag_width);
                if (tag == h2) {
                    altpred_indx = provider_indx;
                    provider_indx = i;
                }
            }

            return m_T_pred[provider_indx];
        }

        void update(bool takenActually, bool takenPredicted, ADDRINT addr)
        {
            GlobalHistoryPredictor<hash1>* ghp = (GlobalHistoryPredictor<hash1>*) m_T[provider_indx];

            // TODO: Update provider itself
            m_T[provider_indx]->update(takenActually, takenPredicted, addr);


            // TODO: Update usefulness
            // ��Ԥ���� T0 û�з��� m_useful ���ڴ�, ��Ҫ����
            if (m_T_pred[provider_indx] != m_T_pred[altpred_indx] && (provider_indx != 0)) {
                int idx = ghp->get_tag(addr);
                if (m_T_pred[provider_indx] == takenActually) {
                    m_useful[provider_indx][idx]++;
                } else {
                    if (m_useful[provider_indx][idx] > 0) {
                        m_useful[provider_indx][idx]--;
                    }
                }
            }

            // TODO: Reset usefulness periodically
            m_rst_cnt++;
            if (m_rst_cnt == m_rst_period) {
                for (size_t i = 1; i < m_tnum; i++)
                {
                    memset(m_useful[i], 0, sizeof(UINT8)*(1 << m_entries_log));
                }
                m_rst_cnt = 0;
            }

            // TODO: Entry replacement
            bool find = false;
            for (size_t i = provider_indx + 1; i < m_tnum; i++) {
                GlobalHistoryPredictor<hash1>* ghp_i = (GlobalHistoryPredictor<hash1>*) m_T[i];
                UINT128 h2 = hash2(addr, ghp_i->get_ghr());
                if (m_useful[i][ghp_i->get_tag(addr)] == 0) {
                    m_tag[i][ghp_i->get_tag(addr)] = truncate(h2, m_tag_width);
                    ghp_i->reset_ctr(addr);
                    find = true;
                }
            }

            if (find == false) {
                for (size_t i = provider_indx + 1; i < m_tnum; i++)
                {
                    GlobalHistoryPredictor<hash1>* ghp_i = (GlobalHistoryPredictor<hash1>*) m_T[i];
                    if (m_useful[i][ghp_i->get_tag(addr)] > 0) {
                        m_useful[i][ghp_i->get_tag(addr)]--;
                    }
                }
            }
        }
};



// This function is called every time a control-flow instruction is encountered
void predictBranch(ADDRINT pc, BOOL direction)
{
    BOOL prediction = BP->predict(pc);
    BP->update(direction, prediction, pc);
    if (prediction)
    {
        if (direction)
            takenCorrect++;
        else
            takenIncorrect++;
    }
    else
    {
        if (direction)
            notTakenIncorrect++;
        else
            notTakenCorrect++;
    }
}

// Pin calls this function every time a new instruction is encountered
void Instruction(INS ins, void * v)
{
    if (INS_IsControlFlow(ins) && INS_HasFallThrough(ins))
    {
        // Insert a call to the branch target
        INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)predictBranch,
                        IARG_INST_PTR, IARG_BOOL, TRUE, IARG_END);

        // Insert a call to the next instruction of a branch
        INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)predictBranch,
                        IARG_INST_PTR, IARG_BOOL, FALSE, IARG_END);
    }
}

// This knob sets the output file name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "brchPredict.txt", "specify the output file name");

// This function is called when the application exits
VOID Fini(int, VOID * v)
{
	double precision = 100 * double(takenCorrect + notTakenCorrect) / (takenCorrect + notTakenCorrect + takenIncorrect + notTakenIncorrect);
    
    cout << "takenCorrect: " << takenCorrect << endl
    	<< "takenIncorrect: " << takenIncorrect << endl
    	<< "notTakenCorrect: " << notTakenCorrect << endl
    	<< "nnotTakenIncorrect: " << notTakenIncorrect << endl
    	<< "Precision: " << precision << endl;
    
    OutFile.setf(ios::showbase);
    OutFile << "takenCorrect: " << takenCorrect << endl
    	<< "takenIncorrect: " << takenIncorrect << endl
    	<< "notTakenCorrect: " << notTakenCorrect << endl
    	<< "nnotTakenIncorrect: " << notTakenIncorrect << endl
    	<< "Precision: " << precision << endl;
    
    OutFile.close();
    delete BP;
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // TODO: New your Predictor below.
    BP = new BHTPredictor(15); // ���� BHT �ķ�֧Ԥ��
    BP = new GlobalHistoryPredictor<f_xnor>(25, 15); // ����ȫ����ʷ�ķ�֧Ԥ��
    BranchPredictor* BP0 = new GlobalHistoryPredictor<f_xor>(25, 15);
    BranchPredictor* BP1 = new GlobalHistoryPredictor<f_xor1>(20, 15);
    BP = new TournamentPredictor(BP0, BP1); // ��������֧Ԥ��
    BP = new TAGEPredictor<f_xnor, f_xor>(3, 12, 25, 5, 15, 2); // ���� Tage �ķ�֧Ԥ��

    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();
    
    OutFile.open(KnobOutputFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
