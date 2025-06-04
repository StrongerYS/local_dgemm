#include "local_dgemm_manager.h"

#include "internal/arch_aurora.h"
// #define ARCH_AURORA

LocalDgemmManager::LocalDgemmManager() {
    #ifdef ARCH_AURORA
    bufInput[0].inputA[0] = reinterpret_cast<void *>(0x380000); // A_UP
    bufInput[0].inputA[1] = reinterpret_cast<void *>(0x680000); // A_DOWN
    bufInput[0].inputB[0] = reinterpret_cast<void *>(0x3c0000); // B_LEFT
    bufInput[0].inputB[1] = reinterpret_cast<void *>(0x6c0000); // B_RIGHT

    bufInput[1].inputA[0] = reinterpret_cast<void *>(0x400000); // A_UP
    bufInput[1].inputA[1] = reinterpret_cast<void *>(0x700000); // A_DOWN
    bufInput[1].inputB[0] = reinterpret_cast<void *>(0x440000); // B_LEFT
    bufInput[1].inputB[1] = reinterpret_cast<void *>(0x740000); // B_RIGHT

    bufInput[2].inputA[0] = reinterpret_cast<void *>(0x480000); // A_UP
    bufInput[2].inputA[1] = reinterpret_cast<void *>(0x780000); // A_DOWN
    bufInput[2].inputB[0] = reinterpret_cast<void *>(0x4c0000); // B_LEFT
    bufInput[2].inputB[1] = reinterpret_cast<void *>(0x7c0000); // B_RIGHT

    bufOutput[0].outputC[0] = reinterpret_cast<void *>(0x580000); // C00
    bufOutput[0].outputC[1] = reinterpret_cast<void *>(0x580000 + DM_BANK_SIZE / 2); // C01
    bufOutput[0].outputC[2] = reinterpret_cast<void *>(0x5c0000); // C10
    bufOutput[0].outputC[3] = reinterpret_cast<void *>(0x5c0000 + DM_BANK_SIZE / 2); // C11

    bufOutput[1].outputC[0] = reinterpret_cast<void *>(0x600000); // C00
    bufOutput[1].outputC[1] = reinterpret_cast<void *>(0x600000 + DM_BANK_SIZE / 2); // C01
    bufOutput[1].outputC[2] = reinterpret_cast<void *>(0x640000); // C10
    bufOutput[1].outputC[3] = reinterpret_cast<void *>(0x640000 + DM_BANK_SIZE / 2); // C11
    #else
    for (int i = 0; i < NumBufferInput; ++i) {
        bufInput[i].inputA[0] = new char[DM_BANK_SIZE]; // A_UP
        bufInput[i].inputA[1] = new char[DM_BANK_SIZE]; // A_DOWN
        bufInput[i].inputB[0] = new char[DM_BANK_SIZE]; // B_LEFT
        bufInput[i].inputB[1] = new char[DM_BANK_SIZE]; // B_RIGHT
    }

    for (int i = 0; i < NumBufferOutput; ++i) {
        bufOutput[i].outputC[0] = new char[DM_BANK_SIZE / 2]; // C00
        bufOutput[i].outputC[1] = new char[DM_BANK_SIZE / 2]; // C01
        bufOutput[i].outputC[2] = new char[DM_BANK_SIZE / 2]; // C10
        bufOutput[i].outputC[3] = new char[DM_BANK_SIZE / 2]; // C11
    }
    #endif
}
LocalDgemmManager::~LocalDgemmManager() {
    #ifdef ARCH_AURORA
    #else
    for (int i = 0; i < NumBufferInput; ++i) {
        delete[] static_cast<char *>(bufInput[i].inputA[0]);
        delete[] static_cast<char *>(bufInput[i].inputA[1]);
        delete[] static_cast<char *>(bufInput[i].inputB[0]);
        delete[] static_cast<char *>(bufInput[i].inputB[1]);
    }
    for (int i = 0; i < NumBufferOutput; ++i) {
        delete[] static_cast<char *>(bufOutput[i].outputC[0]);
        delete[] static_cast<char *>(bufOutput[i].outputC[1]);
        delete[] static_cast<char *>(bufOutput[i].outputC[2]);
        delete[] static_cast<char *>(bufOutput[i].outputC[3]);
    }
    #endif
}

void LocalDgemmManager::confTaskInfo(const int &m, const int &n, const int &k, const bool &acc, const unsigned int &taskId) {
    taskInfo[taskId].m = m;
    taskInfo[taskId].n = n;
    taskInfo[taskId].k = k;
    taskInfo[taskId].m0 = m >> 1;
    taskInfo[taskId].m1 = m - taskInfo[taskId].m0;
    taskInfo[taskId].m0Pad = align8(taskInfo[taskId].m0);
    taskInfo[taskId].m1Pad = align8(taskInfo[taskId].m1);
    taskInfo[taskId].n0 = n >> 1;
    taskInfo[taskId].n1 = n - taskInfo[taskId].n0;
    taskInfo[taskId].n0Pad = align8(taskInfo[taskId].n0);
    taskInfo[taskId].n1Pad = align8(taskInfo[taskId].n1);
    taskInfo[taskId].kPad = align8(k);

    taskInfo[taskId].bufInputId = bufInputIdleId;
    bufInputIdleId = (bufInputIdleId + 1) % NumBufferInput;
    if(acc){
        taskInfo[taskId].bufOutputId = taskInfo[taskCurrentId].bufOutputId; // Use the current task's output buffer for accumulation
    }else{
        taskInfo[taskId].bufOutputId = bufOutputIdleId;
        bufOutputIdleId = (bufOutputIdleId + 1) % NumBufferOutput;
    }
    taskInfo[taskId].acc = acc;

    taskInfo[taskId].status = TaskStatus::Ready;
}
void LocalDgemmManager::calculate(const unsigned int &m, const unsigned int &n, const unsigned int &k, const bool &acc, const void *inputA, const void *inputB, void *outputC) {
    // Implement the calculation logic here using inputA, inputB, and outputC
    const double* A = static_cast<const double*>(inputA);
    const double* B = static_cast<const double*>(inputB);
    double* C = static_cast<double*>(outputC);

    for (unsigned int i = 0; i < m; ++i) {
        for (unsigned int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (unsigned int p = 0; p < k; ++p) {
                sum += A[i * k + p] * B[p * n + j];
            }
            if (acc) {
                C[i * n + j] += sum;
            } else {
                C[i * n + j] = sum;
            }
        }
    }
}
void LocalDgemmManager::calculate00(const unsigned int &taskId) {
    calculate(
        taskInfo[taskId].m0Pad,
        taskInfo[taskId].n0Pad,
        taskInfo[taskId].kPad,
        taskInfo[taskId].acc,
        bufInput[taskInfo[taskId].bufInputId].inputA[0], // A_UP
        bufInput[taskInfo[taskId].bufInputId].inputB[0], // B_LEFT
        bufOutput[taskInfo[taskId].bufOutputId].outputC[0] // C00
    );  
}
void LocalDgemmManager::calculate01(const unsigned int &taskId) {
    calculate(
        taskInfo[taskId].m0Pad,
        taskInfo[taskId].n1Pad,
        taskInfo[taskId].kPad,
        taskInfo[taskId].acc,
        bufInput[taskInfo[taskId].bufInputId].inputA[0], // A_UP
        bufInput[taskInfo[taskId].bufInputId].inputB[1], // B_RIGHT
        bufOutput[taskInfo[taskId].bufOutputId].outputC[1] // C01
    );  
}
void LocalDgemmManager::calculate10(const unsigned int &taskId) {
    calculate(
        taskInfo[taskId].m1Pad,
        taskInfo[taskId].n0Pad,
        taskInfo[taskId].kPad,
        taskInfo[taskId].acc,
        bufInput[taskInfo[taskId].bufInputId].inputA[1], // A_DOWN
        bufInput[taskInfo[taskId].bufInputId].inputB[0], // B_LEFT
        bufOutput[taskInfo[taskId].bufOutputId].outputC[2] // C10
    );  
}
void LocalDgemmManager::calculate11(const unsigned int &taskId) {
    calculate(
        taskInfo[taskId].m1Pad,
        taskInfo[taskId].n1Pad,
        taskInfo[taskId].kPad,
        taskInfo[taskId].acc,
        bufInput[taskInfo[taskId].bufInputId].inputA[1], // A_DOWN
        bufInput[taskInfo[taskId].bufInputId].inputB[1], // B_RIGHT
        bufOutput[taskInfo[taskId].bufOutputId].outputC[3] // C11
    );  
}

void LocalDgemmManager::reset(){
    bufInputIdleId = 0;
    bufOutputIdleId = 0;
    taskCurrentId = 0;
    taskNextId = 0;
    for(auto &info : taskInfo){
        info.status = TaskStatus::Idle;
    }
}
bool LocalDgemmManager::registerTask(const int &m, const int &n, const int &k, const bool &acc, unsigned int &taskId) {
    constexpr int maxM = 256; ///< sqrt(DM_BANK_SIZE * 2 / sizeof(double));
    constexpr int maxN = maxM;

    if (taskInfo[taskNextId].status != TaskStatus::Idle) {
        return false; // No available task slot
    }

    if (m <= 0 || m > maxM || n <= 0 || n > maxN || k <= 0 || 
        (m * k * sizeof(double) > DM_BANK_SIZE * 2) || (n * k * sizeof(double) > DM_BANK_SIZE * 2) || (m * n * sizeof(double) > DM_BANK_SIZE * 2)) {
        return false;
    }

    if (acc){
        if(m != taskInfo[taskCurrentId].m || n != taskInfo[taskCurrentId].n){
            return false; // Cannot accumulate with different dimensions
        }

    }

    taskId = taskNextId;
    confTaskInfo(m, n, k, acc, taskId);

    taskCurrentId = taskNextId;
    taskNextId = (taskNextId + 1) % NumTasks;

    return true;

}
bool LocalDgemmManager::getTaskInfo(const unsigned int &taskId, LocalDgemmTaskInfo &taskInfo) {
    if (taskId < NumTasks && this->taskInfo[taskId].status != TaskStatus::Idle) {
        taskInfo = this->taskInfo[taskId];
        return true;
    }
    return false;
}
MatrixDmaParam LocalDgemmManager::getTaskMatrixDmaParamInputA(const unsigned int &taskId, const int &blkId) {
    MatrixDmaParam param;
    if (taskId < NumTasks && blkId >= 0 && blkId < 2) {
        param.addr = bufInput[taskInfo[taskId].bufInputId].inputA[blkId];
        param.xnum = taskInfo[taskId].k * sizeof(double);
        param.ynum = (0 == blkId) ? taskInfo[taskId].m0 : taskInfo[taskId].m1;
        param.ystep = taskInfo[taskId].kPad * sizeof(double); // Step size for y direction
    } else {
        param.addr = nullptr; // Invalid taskId or blkId
    }
    return param;
}
MatrixDmaParam LocalDgemmManager::getTaskMatrixDmaParamInputB(const unsigned int &taskId, const int &blkId) {
    MatrixDmaParam param;
    if (taskId < NumTasks && blkId >= 0 && blkId < 2) {
        param.addr = bufInput[taskInfo[taskId].bufInputId].inputB[blkId];
        param.xnum = ((0 == blkId) ? taskInfo[taskId].n0 : taskInfo[taskId].n1) * sizeof(double);
        param.ynum = taskInfo[taskId].k;
        param.ystep = ((0 == blkId) ? taskInfo[taskId].n0Pad : taskInfo[taskId].n1Pad) * sizeof(double);
    } else {
        param.addr = nullptr; // Invalid taskId or blkId
    }
    return param;
}
MatrixDmaParam LocalDgemmManager::getTaskMatrixDmaParamOutputC(const unsigned int &taskId, const int &blkId) {
    MatrixDmaParam param;
    if (taskId < NumTasks && blkId >= 0 && blkId < 4) {
        param.addr = bufOutput[taskInfo[taskId].bufOutputId].outputC[blkId];
        param.xnum = ((blkId == 0 || blkId == 2) ? taskInfo[taskId].n0 : taskInfo[taskId].n1) * sizeof(double);
        param.ynum = ((blkId == 0 || blkId == 1) ? taskInfo[taskId].m0 : taskInfo[taskId].m1);
        param.ystep = ((blkId == 0 || blkId == 2) ? taskInfo[taskId].n0Pad : taskInfo[taskId].n1Pad) * sizeof(double);
    } else {
        param.addr = nullptr; // Invalid taskId or blkId
    }
    return param;
}
// void *LocalDgemmManager::getTaskBufferInput(const unsigned int &taskId, const int &bankId, const bool &isA) {
//     if (taskId < NumTasks && bankId >= 0 && bankId < 2) {
//         if (isA) {
//             return taskInfo[taskId].bufInputId < NumBufferInput ? 
//                    bufInput[taskInfo[taskId].bufInputId].inputA[bankId] : nullptr;
//         } else {
//             return taskInfo[taskId].bufInputId < NumBufferInput ? 
//                    bufInput[taskInfo[taskId].bufInputId].inputB[bankId] : nullptr;
//         }
//     }
//     return nullptr; // Invalid taskId or bankId
// }
void LocalDgemmManager::alignBufferInput(const unsigned int &taskId) {
    if (taskId < NumTasks) {
        // Align the input buffers for the task
        auto &task = taskInfo[taskId];
        if (task.bufInputId < NumBufferInput) {
            double *bLeft = reinterpret_cast<double *>(
                static_cast<char *>(bufInput[task.bufInputId].inputB[0]) + task.n0Pad * task.k * sizeof(double));
            double *bRight = reinterpret_cast<double *>(
                static_cast<char *>(bufInput[task.bufInputId].inputB[1]) + task.n1Pad * task.k * sizeof(double));
            for (int i = 0; i < (task.kPad - task.k); ++i){
                for (int j = 0; j < task.n0; ++j) {
                    bLeft[i * task.n0Pad + j] = 0.0; // Fill B_LEFT with zeros
                }
            }
            for (int i = 0; i < (task.kPad - task.k); ++i){
                for (int j = 0; j < task.n1; ++j) {
                    bRight[i * task.n1Pad + j] = 0.0; // Fill B_RIGHT with zeros
                }
            }
        }
    }
}
bool LocalDgemmManager::markTaskDataReady(const unsigned int &taskId) {
    if (taskId < NumTasks && taskInfo[taskId].status == TaskStatus::Ready) {
        taskInfo[taskId].status = TaskStatus::DataReady;
        return true;
    }
    return false;
}
bool LocalDgemmManager::getTaskStatus(const unsigned int &taskId, TaskStatus &status) {
    if (taskId < NumTasks) {
        status = this->taskInfo[taskId].status;
        return true;
    }
    return false;
}
bool LocalDgemmManager::stepAllTask() {
    bool stepRet = false;
    unsigned int cal0TaskId = NumTasks;
    unsigned int cal1TaskId = NumTasks;

    for (unsigned int i = 0; i < NumTasks; ++i) {
        if (taskInfo[i].status == TaskStatus::Idle || taskInfo[i].status == TaskStatus::Ready) {
            continue; // Skip idle tasks
        } else if (taskInfo[i].status == TaskStatus::DataReady) {
            if(NumTasks == cal0TaskId){
                taskInfo[i].status = TaskStatus::Calculate0;
                cal0TaskId = i;
                stepRet = true;
            }else{
                stepRet = false; // Only one task can be in Calculate0 state at a time
                break;
            }
        } else if (taskInfo[i].status == TaskStatus::Calculate0) {
            if(NumTasks == cal1TaskId){
                taskInfo[i].status = TaskStatus::Calculate1;
                cal1TaskId = i;
                stepRet = true;
            }else{
                stepRet = false; // Only one task can be in Calculate1 state at a time
                break;
            }
        } else if (taskInfo[i].status == TaskStatus::Calculate1) {
            taskInfo[i].status = TaskStatus::Finished;
            stepRet = true;
        }
    }
    if (stepRet) {
        if (cal0TaskId < NumTasks){
            calculate00(cal0TaskId);
            calculate11(cal0TaskId);
        }
        if (cal1TaskId < NumTasks){
            calculate01(cal1TaskId);
            calculate10(cal1TaskId);
        }
    }
    
    return stepRet;
}
bool LocalDgemmManager::endTask(const unsigned int &taskId) {
    if (taskId < NumTasks && taskInfo[taskId].status == TaskStatus::Finished) {
        taskInfo[taskId].status = TaskStatus::Idle;
        return true;
    }
    return false;
}
        