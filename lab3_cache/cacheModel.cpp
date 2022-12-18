#include <cstdio>
#include <cmath>
#include <ctime>
#include "pin.H"

typedef unsigned int        UINT32;
typedef unsigned long int   UINT64;


#define PAGE_SIZE_LOG       12
#define PHY_MEM_SIZE_LOG    30

#define get_vir_page_no(virtual_addr)   (virtual_addr >> PAGE_SIZE_LOG)
#define get_page_offset(addr)           (addr & ((1u << PAGE_SIZE_LOG) - 1))

// Obtain physical page number according to a given virtual page number
UINT32 get_phy_page_no(UINT32 virtual_page_no)
{
    UINT32 vpn = virtual_page_no;
    vpn = (~vpn ^ (vpn << 16)) + (vpn & (vpn << 16)) + (~vpn | (vpn << 2));

    UINT32 mask = (UINT32)(~0) << (32 - PHY_MEM_SIZE_LOG);
    mask = mask >> (32 - PHY_MEM_SIZE_LOG + PAGE_SIZE_LOG);
    mask = mask << PAGE_SIZE_LOG;

    return vpn & mask;
}

// Transform a virtual address into a physical address
UINT32 get_phy_addr(UINT32 virtual_addr)
{
    return (get_phy_page_no(get_vir_page_no(virtual_addr)) << PAGE_SIZE_LOG) + get_page_offset(virtual_addr);
}

/**************************************
 * Cache Model Base Class
**************************************/
class CacheModel
{
public:
    // Constructor
    CacheModel(UINT32 block_num, UINT32 log_block_size)
        : m_block_num(block_num), m_blksz_log(log_block_size),
          m_rd_reqs(0), m_wr_reqs(0), m_rd_hits(0), m_wr_hits(0)
    {
        m_valids = new bool[m_block_num];
        m_tags = new UINT32[m_block_num];
        m_replace_q = new UINT32[m_block_num];

        for (UINT i = 0; i < m_block_num; i++)
        {
            m_valids[i] = false;
            m_replace_q[i] = i;
        }
    }

    // Destructor
    virtual ~CacheModel()
    {
        delete[] m_valids;
        delete[] m_tags;
        delete[] m_replace_q;
    }

    // Update the cache state whenever data is read
    void readReq(UINT32 mem_addr)
    {
        m_rd_reqs++;
        if (access(mem_addr)) m_rd_hits++;
    }

    // Update the cache state whenever data is written
    void writeReq(UINT32 mem_addr)
    {
        m_wr_reqs++;
        if (access(mem_addr)) m_wr_hits++;
    }

    UINT32 getRdReq() { return m_rd_reqs; }
    UINT32 getWrReq() { return m_wr_reqs; }

    void dumpResults()
    {
        float rdHitRate = 100 * (float)m_rd_hits/m_rd_reqs;
        float wrHitRate = 100 * (float)m_wr_hits/m_wr_reqs;
        printf("\tread req: %lu,\thit: %lu,\thit rate: %.2f%%\n", m_rd_reqs, m_rd_hits, rdHitRate);
        printf("\twrite req: %lu,\thit: %lu,\thit rate: %.2f%%\n", m_wr_reqs, m_wr_hits, wrHitRate);
    }

protected:
    UINT32 m_block_num;     // The number of cache blocks
    UINT32 m_blksz_log;     // 块大小的对数

    bool* m_valids;
    UINT32* m_tags;
    UINT32* m_replace_q;    // Cache块替换的候选队列

    UINT64 m_rd_reqs;       // The number of read-requests
    UINT64 m_wr_reqs;       // The number of write-requests
    UINT64 m_rd_hits;       // The number of hit read-requests
    UINT64 m_wr_hits;       // The number of hit write-requests

    // Look up the cache to decide whether the access is hit or missed
    virtual bool lookup(UINT32 mem_addr, UINT32& blk_id) = 0;

    // Access the cache: update m_replace_q if hit, otherwise replace a block and update m_replace_q
    virtual bool access(UINT32 mem_addr) = 0;

    // Update m_replace_q
    virtual void updateReplaceQ(UINT32 blk_id) = 0;
};

/**************************************
 * Fully Associative Cache Class
**************************************/
class FullAssoCache : public CacheModel
{
public:
    // Constructor
    FullAssoCache(UINT32 block_num, UINT32 log_block_size)
        : CacheModel(block_num, log_block_size) {}

    // Destructor
    ~FullAssoCache() {}

private:
    UINT32 getTag(UINT32 addr) { /* TODO */ return addr >> m_blksz_log; }

    // Look up the cache to decide whether the access is hit or missed
    bool lookup(UINT32 mem_addr, UINT32& blk_id)
    {
        UINT32 tag = getTag(mem_addr);

        // TODO
        for (UINT32 i = 0; i < m_block_num; i++)
        {
            if ((m_tags[i] == tag) && (m_valids[i] == true))
            {
                blk_id = i;
                return true;
            }
        }

        return false;
    }

    // Access the cache: update m_replace_q if hit, otherwise replace a block and update m_replace_q
    bool access(UINT32 mem_addr)
    {
        UINT32 blk_id;
        if (lookup(mem_addr, blk_id))
        {
            updateReplaceQ(blk_id);     // Update m_replace_q
            return true;
        }

        // Get the to-be-replaced block id using m_replace_q
        UINT32 bid_2be_replaced = m_replace_q[0]; // TODO

        // Replace the cache block
        // TODO
        m_tags[bid_2be_replaced] = getTag(mem_addr);
        m_valids[bid_2be_replaced] = true;
        updateReplaceQ(bid_2be_replaced);

        return false;
    }

    // Update m_replace_q
    void updateReplaceQ(UINT32 blk_id)
    {
        // TODO
        for (UINT32 i = 0; i < m_block_num; i++)
        {
            if (m_replace_q[i] == blk_id)
            {
                for (UINT32 j = i + 1; j < m_block_num; j++)
                {
                    m_replace_q[j - 1] = m_replace_q[j];
                }
                m_replace_q[m_block_num - 1] = blk_id;
                break;
            }
        }
    }
};

/**************************************
 * Set-Associative Cache Class
**************************************/
class SetAssoCache : public CacheModel
{
public:
    // Constructor
    SetAssoCache(/* TODO */ UINT32 log_sets, UINT32 log_block_size, UINT32 asso)
        : CacheModel((asso * (1 << log_sets)), log_block_size)
    {
        m_sets_log = log_sets;
        m_asso = asso;
    }

    // Destructor
    ~SetAssoCache() {}

protected:

    // the log of the number of rows & the m_asso
    UINT32 m_sets_log;
    UINT32 m_asso;

    // 获得当前主存地址的区内块号
    virtual UINT32 getIndex(UINT32 addr) {
        return (addr >> m_blksz_log) & ((1 << m_sets_log) - 1);
    }

    // 获得当前主存地址的区号
    virtual UINT32 getTag(UINT32 addr) {
        return (addr >> (m_blksz_log + m_sets_log));
    }

    // Look up the cache to decide whether the access is hit or missed
    bool lookup(UINT32 mem_addr, UINT32& blk_id)
    {
        // TODO
        UINT32 index = getIndex(mem_addr);
        UINT32 tag = getTag(mem_addr);

        for (UINT32 i = (index * m_asso); i < (index * m_asso + m_asso); i++)
        {
            if ((m_tags[i] == tag) && (m_valids[i] == true))
            {
                blk_id = i;
                return true;
            }
        }

        return false;
    }

    // Access the cache: update m_replace_q if hit, otherwise replace a block and update m_replace_q
    bool access(UINT32 mem_addr)
    {
        // TODO
        UINT32 blk_id;

        if (lookup(mem_addr, blk_id))
        {
            updateReplaceQ(blk_id);     // Update m_replace_q
            return true;
        }

        // Get the to-be-replaced block id using m_replace_q
        UINT32 bid_2be_replaced = getIndex(mem_addr) * m_asso;
        for (UINT32 i = 0; i < m_block_num; i++)
        {
            if (m_replace_q[i] / m_asso == getIndex(mem_addr)) {
                bid_2be_replaced = m_replace_q[i];
                break;
            }
        }

        // Replace the cache block
        m_tags[bid_2be_replaced] = getTag(mem_addr);
        m_valids[bid_2be_replaced] = true;
        updateReplaceQ(bid_2be_replaced);

        return false;
    }

    // Update m_replace_q
    void updateReplaceQ(UINT32 blk_id)
    {
        // TODO
        for (UINT32 i = 0; i < m_block_num; i++)
        {
            if (m_replace_q[i] == blk_id)
            {
                for (UINT32 j = i + 1; j < m_block_num; j++)
                {
                    m_replace_q[j - 1] = m_replace_q[j];
                }
                m_replace_q[m_block_num - 1] = blk_id;
                break;
            }
        }
    }
};

/**************************************
 * Set-Associative Cache Class (VIVT)
**************************************/
class SetAssoCache_VIVT : public SetAssoCache
{
public:
    // Constructor
    SetAssoCache_VIVT(/* TODO */ UINT32 log_sets, UINT32 log_block_size, UINT32 asso)
        : SetAssoCache(log_sets, log_block_size, asso) {}

    // Destructor
    ~SetAssoCache_VIVT() {}

private:

    /* 直接继承 SetAssoCache 类的成员变量及方法, 无需重复实现 */

    // Add your members

    // // Look up the cache to decide whether the access is hit or missed
    // bool lookup(UINT32 mem_addr, UINT32& blk_id)
    // {
    //     // TODO
    // }

    // // Access the cache: update m_replace_q if hit, otherwise replace a block and update m_replace_q
    // bool access(UINT32 mem_addr)
    // {
    //     // TODO
    // }

    // // Update m_replace_q
    // void updateReplaceQ(UINT32 blk_id)
    // {
    //     // TODO
    // }
};

/**************************************
 * Set-Associative Cache Class (PIPT)
**************************************/
class SetAssoCache_PIPT : public SetAssoCache
{
public:
    // Constructor
    SetAssoCache_PIPT(/* TODO */ UINT32 log_sets, UINT32 log_block_size, UINT32 asso)
        : SetAssoCache(log_sets, log_block_size, asso) {}

    // Destructor
    ~SetAssoCache_PIPT() {}

private:

    /* 除了 getIndex 和 getTag 方法需要修改为根据物理地址来获取 index 和 tag, 其它成员变量和方法则是直接继承自 SetAssoCache 类, 无需重复实现 */

    // Add your members

    // 获得当前主存地址的区内块号
    UINT32 getIndex(UINT32 addr) {
        UINT32 paddr = get_phy_addr(addr);
        return (paddr >> m_blksz_log) & ((1 << m_sets_log) - 1);
    }

    // 获得当前主存地址的区号
    UINT32 getTag(UINT32 addr) {
        UINT32 paddr = get_phy_addr(addr);
        return (paddr >> (m_blksz_log + m_sets_log));
    }

    // // Look up the cache to decide whether the access is hit or missed
    // bool lookup(UINT32 mem_addr, UINT32& blk_id)
    // {
    //     // TODO
    // }

    // // Access the cache: update m_replace_q if hit, otherwise replace a block and update m_replace_q
    // bool access(UINT32 mem_addr)
    // {
    //     // TODO
    // }

    // // Update m_replace_q
    // void updateReplaceQ(UINT32 blk_id)
    // {
    //     // TODO
    // }
};

/**************************************
 * Set-Associative Cache Class (VIPT)
**************************************/
class SetAssoCache_VIPT : public SetAssoCache
{
public:
    // Constructor
    SetAssoCache_VIPT(/* TODO */ UINT32 log_sets, UINT32 log_block_size, UINT32 asso)
        : SetAssoCache(log_sets, log_block_size, asso) {}

    // Destructor
    ~SetAssoCache_VIPT() {}

private:

    /* 除了 getTag 方法需要修改为根据物理地址来获取 tag, 其它成员变量和方法则是直接继承自 SetAssoCache 类, 无需重复实现 */

    // Add your members

    // 获得当前主存地址的区号
    UINT32 getTag(UINT32 addr) {
        UINT32 paddr = get_phy_addr(addr);
        return (paddr >> (m_blksz_log + m_sets_log));
    }

    // // Look up the cache to decide whether the access is hit or missed
    // bool lookup(UINT32 mem_addr, UINT32& blk_id)
    // {
    //     // TODO
    // }

    // // Access the cache: update m_replace_q if hit, otherwise replace a block and update m_replace_q
    // bool access(UINT32 mem_addr)
    // {
    //     // TODO
    // }

    // // Update m_replace_q
    // void updateReplaceQ(UINT32 blk_id)
    // {
    //     // TODO
    // }
};

CacheModel* my_fa_cache;
CacheModel* my_sa_cache;
CacheModel* my_sa_cache_vivt;
CacheModel* my_sa_cache_pipt;
CacheModel* my_sa_cache_vipt;

// Cache reading analysis routine
void readCache(UINT32 mem_addr)
{
    mem_addr = (mem_addr >> 2) << 2;

    my_fa_cache->readReq(mem_addr);
    my_sa_cache->readReq(mem_addr);

    my_sa_cache_vivt->readReq(mem_addr);
    my_sa_cache_pipt->readReq(mem_addr);
    my_sa_cache_vipt->readReq(mem_addr);
}

// Cache writing analysis routine
void writeCache(UINT32 mem_addr)
{
    mem_addr = (mem_addr >> 2) << 2;

    my_fa_cache->writeReq(mem_addr);
    my_sa_cache->writeReq(mem_addr);

    my_sa_cache_vivt->writeReq(mem_addr);
    my_sa_cache_pipt->writeReq(mem_addr);
    my_sa_cache_vipt->writeReq(mem_addr);
}

// This knob will set the cache param m_block_num
KNOB<UINT32> KnobBlockNum(KNOB_MODE_WRITEONCE, "pintool",
        "n", "512", "specify the number of blocks in bytes");

// This knob will set the cache param m_blksz_log
KNOB<UINT32> KnobBlockSizeLog(KNOB_MODE_WRITEONCE, "pintool",
        "b", "6", "specify the log of the block size in bytes");

// This knob will set the cache param m_sets_log
KNOB<UINT32> KnobSetsLog(KNOB_MODE_WRITEONCE, "pintool",
        "r", "7", "specify the log of the number of rows");

// This knob will set the cache param m_asso
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
        "a", "4", "specify the m_asso");

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)readCache, IARG_MEMORYREAD_EA, IARG_END);
    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeCache, IARG_MEMORYWRITE_EA, IARG_END);
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    printf("\nFully Associative Cache:\n");
    my_fa_cache->dumpResults();

    printf("\nSet-Associative Cache:\n");
    my_sa_cache->dumpResults();

    printf("\nSet-Associative Cache (VIVT):\n");
    my_sa_cache_vivt->dumpResults();

    printf("\nSet-Associative Cache (PIPT):\n");
    my_sa_cache_pipt->dumpResults();

    printf("\nSet-Associative Cache (VIPT):\n");
    my_sa_cache_vipt->dumpResults();

    delete my_fa_cache;
    delete my_sa_cache;

    delete my_sa_cache_vivt;
    delete my_sa_cache_pipt;
    delete my_sa_cache_vipt;
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char* argv[])
{
    // Initialize pin
    PIN_Init(argc, argv);

    my_fa_cache = new FullAssoCache(KnobBlockNum.Value(), KnobBlockSizeLog.Value());
    my_sa_cache = new SetAssoCache(KnobSetsLog.Value(), KnobBlockSizeLog.Value(), KnobAssociativity.Value());

    my_sa_cache_vivt = new SetAssoCache_VIVT(KnobSetsLog.Value(), KnobBlockSizeLog.Value(), KnobAssociativity.Value());
    my_sa_cache_pipt = new SetAssoCache_PIPT(KnobSetsLog.Value(), KnobBlockSizeLog.Value(), KnobAssociativity.Value());
    my_sa_cache_vipt = new SetAssoCache_VIPT(KnobSetsLog.Value(), KnobBlockSizeLog.Value(), KnobAssociativity.Value());

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
