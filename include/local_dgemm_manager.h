#ifndef LOCAL_DGEMM_MANAGER_H
#define LOCAL_DGEMM_MANAGER_H

struct DgemmBufferInput {
    void* inputA[2];    /// A_UP, A_DOWN
    void* inputB[2];    /// B_LEFT, B_RIGHT
};
struct DgemmBufferOutput {
    void* outputC[4];   /// C00, C01, C10, C11
};

struct LocalBlockInfo{
    int row, col, rowPad, colPad;
    void *addr;
};
enum class TaskStatus {
    Idle = 0,
    Ready = 1,
    DataReady = 2,
    Calculate0 = 3,
    Calculate1 = 4,
    Finished = 5
};
struct LocalDgemmTaskInfo {
    int m, n, k;                    ///< m = rows of A, n = cols of B, k = cols of A / rows of B
    int m0, m1, m0Pad, m1Pad;       ///< m0 = m >> 1, m1 = m - m0
    int n0, n1, n0Pad, n1Pad;       ///< n0 = n >> 1, n1 = n - n0
    int kPad;                       ///< kPad = align8(k)
    unsigned int bufInputId;        ///< Buffer input ID
    unsigned int bufOutputId;       ///< Buffer output ID
    bool acc = false;              ///< Accumulate flag
    TaskStatus status = TaskStatus::Idle;
};

struct MatrixDmaParam {
    void *addr;
    unsigned int xnum, ystep, ynum;
};

class LocalDgemmManager
{
public:
    static LocalDgemmManager& getInstance() {
        static LocalDgemmManager instance;
        return instance;
    }

    LocalDgemmManager(const LocalDgemmManager&) = delete;
    LocalDgemmManager& operator=(const LocalDgemmManager&) = delete;

    void reset();
    bool registerTask(const int &m, const int &n, const int &k, const bool &acc, unsigned int &taskId);
    bool getTaskInfo(const unsigned int &taskId, LocalDgemmTaskInfo &taskInfo);
    MatrixDmaParam getTaskMatrixDmaParamInputA(const unsigned int &taskId, const int &blkId);
    MatrixDmaParam getTaskMatrixDmaParamInputB(const unsigned int &taskId, const int &blkId);
    MatrixDmaParam getTaskMatrixDmaParamOutputC(const unsigned int &taskId, const int &blkId);

    // void *getTaskBufferInput(const unsigned int &taskId, const int &bankId, const bool &isA);
    void alignBufferInput(const unsigned int &taskId);
    bool markTaskDataReady(const unsigned int &taskId);
    bool getTaskStatus(const unsigned int &taskId, TaskStatus &status);
    bool stepAllTask();
    bool endTask(const unsigned int &taskId);

private:
    LocalDgemmManager();
    ~LocalDgemmManager();
    static inline int align8(const int x) {
        return (x + 7) & ~7;
    }
    void confTaskInfo(const int &m, const int &n, const int &k, const bool &acc, const unsigned int &taskId);
    void calculate(const unsigned int &m, const unsigned int &n, const unsigned int &k, const bool &acc, 
                   const void *inputA, const void *inputB, void *outputC);
    void calculate00(const unsigned int &taskId);
    void calculate01(const unsigned int &taskId);
    void calculate10(const unsigned int &taskId);
    void calculate11(const unsigned int &taskId);
    
    static constexpr int NumBufferInput = 3;
    static constexpr int NumBufferOutput = 2;
    static constexpr int NumTasks = NumBufferInput;

    DgemmBufferInput bufInput[NumBufferInput];
    DgemmBufferOutput bufOutput[NumBufferOutput];

    LocalDgemmTaskInfo taskInfo[NumTasks];
    unsigned int bufInputIdleId = 0;
    unsigned int bufOutputIdleId = 0;
    unsigned int taskCurrentId = 0;
    unsigned int taskNextId = 0;
};

#endif // LOCAL_DGEMM_MANAGER_H