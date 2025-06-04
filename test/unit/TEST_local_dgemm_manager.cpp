#include "test_local_dgemm_manager.h"

TEST_F(LocalDgemmManagerTest, RegisterTask) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId));
    EXPECT_EQ(taskId, 0u);
}
TEST_F(LocalDgemmManagerTest, RegisterTaskWithInvalidDimensions) {
    unsigned int taskId;
    EXPECT_FALSE(manager->registerTask(0, 64, 64, false, taskId)); // Invalid m
    EXPECT_FALSE(manager->registerTask(64, 0, 64, false, taskId)); // Invalid n
    EXPECT_FALSE(manager->registerTask(64, 64, 0, false, taskId)); // Invalid k
}
TEST_F(LocalDgemmManagerTest, RegisterTaskWithExceedingDimensions) {
    unsigned int taskId;
    EXPECT_FALSE(manager->registerTask(300, 64, 64, false, taskId)); // m exceeds maxM
    EXPECT_FALSE(manager->registerTask(64, 300, 64, false, taskId)); // n exceeds maxN
}
TEST_F(LocalDgemmManagerTest, RegisterTaskWithAccumulation) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, true, taskId));
    EXPECT_EQ(taskId, 0u);
    EXPECT_FALSE(manager->registerTask(32, 64, 64, true, taskId)); // Cannot accumulate with different dimensions
}
TEST_F(LocalDgemmManagerTest, GetTaskInfo) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId));
    LocalDgemmTaskInfo taskInfo;
    EXPECT_TRUE(manager->getTaskInfo(taskId, taskInfo));
    EXPECT_EQ(taskInfo.m, 64);
    EXPECT_EQ(taskInfo.n, 64);
    EXPECT_EQ(taskInfo.k, 64);
}
TEST_F(LocalDgemmManagerTest, GetTaskInfoWithAlignedDimensions) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(61, 61, 61, false, taskId));
    LocalDgemmTaskInfo taskInfo;
    EXPECT_TRUE(manager->getTaskInfo(taskId, taskInfo));
    EXPECT_EQ(taskInfo.m, 61);
    EXPECT_EQ(taskInfo.n, 61);
    EXPECT_EQ(taskInfo.k, 61);
    EXPECT_EQ(taskInfo.m0, 30);
    EXPECT_EQ(taskInfo.m1, 31);
    EXPECT_EQ(taskInfo.m0Pad, 32);
    EXPECT_EQ(taskInfo.m1Pad, 32);
    EXPECT_EQ(taskInfo.n0, 30);
    EXPECT_EQ(taskInfo.n1, 31);
    EXPECT_EQ(taskInfo.n0Pad, 32);
    EXPECT_EQ(taskInfo.n1Pad, 32);
    EXPECT_EQ(taskInfo.kPad, 64);
}
TEST_F(LocalDgemmManagerTest, GetTaskWithInvalidId) {
    LocalDgemmTaskInfo taskInfo;
    EXPECT_FALSE(manager->getTaskInfo(3, taskInfo)); // Invalid taskId
}
TEST_F(LocalDgemmManagerTest, AlignBufferInput) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(61, 61, 61, false, taskId));
    LocalDgemmTaskInfo taskInfo;
    EXPECT_TRUE(manager->getTaskInfo(taskId, taskInfo));
    void *ptBLeft = manager->getTaskMatrixDmaParamInputB(taskId, 0).addr;
    EXPECT_NE(ptBLeft, nullptr);
    void *ptBRight = manager->getTaskMatrixDmaParamInputB(taskId, 1).addr;
    EXPECT_NE(ptBRight, nullptr);
    // 初始化左右B块为1.0
    double* bLeft = static_cast<double*>(ptBLeft);
    double* bRight = static_cast<double*>(ptBRight);
    size_t bLeftSize = taskInfo.kPad * taskInfo.n0Pad;
    size_t bRightSize = taskInfo.kPad * taskInfo.n1Pad;
    std::fill(bLeft, bLeft + bLeftSize, 1.0);
    std::fill(bRight, bRight + bRightSize, 1.0);

    // 调用alignBufferInput进行补零
    manager->alignBufferInput(taskId);

    // 检查bLeft的[k:kPad, 0:n0]部分是否补0
    for (int row = taskInfo.k; row < taskInfo.kPad; ++row) {
        for (int col = 0; col < taskInfo.n0; ++col) {
            EXPECT_EQ(bLeft[row * taskInfo.n0Pad + col], 0.0);
        }
    }
    // 检查bRight的[k:kPad, 0:n1]部分是否补0
    for (int row = taskInfo.k; row < taskInfo.kPad; ++row) {
        for (int col = 0; col < taskInfo.n1; ++col) {
            EXPECT_EQ(bRight[row * taskInfo.n1Pad + col], 0.0);
        }
    }
    
}
TEST_F(LocalDgemmManagerTest, GetTaskStatus) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId));
    TaskStatus status;
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Trans);
}
TEST_F(LocalDgemmManagerTest, GetTaskStatusWithInvalidId) {
    TaskStatus status;
    EXPECT_FALSE(manager->getTaskStatus(3, status)); // Invalid taskId
}
TEST_F(LocalDgemmManagerTest, StepAllTasks) {
    unsigned int taskId0;
    TaskStatus status0;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId0));
    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId0, status0));
    EXPECT_EQ(status0, TaskStatus::Calculate0);

    unsigned int taskId1;
    TaskStatus status1;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId1));
    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId1, status1));
    EXPECT_EQ(status1, TaskStatus::Calculate0);
    EXPECT_TRUE(manager->getTaskStatus(taskId0, status0));
    EXPECT_EQ(status0, TaskStatus::Calculate1);
}
TEST_F(LocalDgemmManagerTest, StepAllTasksWithNoTasks) {
    EXPECT_FALSE(manager->stepAllTask()); // No tasks to step
}
TEST_F(LocalDgemmManagerTest, EndTask) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId));
    TaskStatus status;
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));

    EXPECT_EQ(status, TaskStatus::Trans);
    EXPECT_FALSE(manager->endTask(taskId));

    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Calculate0);
    EXPECT_FALSE(manager->endTask(taskId));

    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Calculate1);
    EXPECT_FALSE(manager->endTask(taskId));

    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Finished);
    EXPECT_TRUE(manager->endTask(taskId)); // Successfully ends the task
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Idle);
    EXPECT_FALSE(manager->endTask(taskId)); // Cannot end a non-existing task
}
TEST_F(LocalDgemmManagerTest, EndTaskWithInvalidId) {
    EXPECT_FALSE(manager->endTask(3)); // Invalid taskId
}
TEST_F(LocalDgemmManagerTest, ResetManager) {
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId));
    manager->reset();
    TaskStatus status;
    EXPECT_TRUE(manager->getTaskStatus(taskId, status)); // Task should be reset
    EXPECT_EQ(status, TaskStatus::Idle);
}

TEST_F(LocalDgemmManagerTest, OneTask){
    unsigned int taskId;
    EXPECT_TRUE(manager->registerTask(64, 64, 64, false, taskId));
    LocalDgemmTaskInfo taskInfo;
    EXPECT_TRUE(manager->getTaskInfo(taskId, taskInfo));
    
    MatrixDmaParam dmaParamAUp = manager->getTaskMatrixDmaParamInputA(taskId, 0);
    MatrixDmaParam dmaParamADown = manager->getTaskMatrixDmaParamInputA(taskId, 1);
    MatrixDmaParam dmaParamBLeft = manager->getTaskMatrixDmaParamInputB(taskId, 0);
    MatrixDmaParam dmaParamBRight = manager->getTaskMatrixDmaParamInputB(taskId, 1);

    std::fill(static_cast<double*>(dmaParamAUp.addr),
              static_cast<double*>(dmaParamAUp.addr) + taskInfo.m0Pad * taskInfo.kPad,
              1.0);
    
    std::fill(static_cast<double*>(dmaParamADown.addr),
              static_cast<double*>(dmaParamADown.addr) + taskInfo.m1Pad * taskInfo.kPad,
              1.0);
    std::fill(static_cast<double*>(dmaParamBLeft.addr),
              static_cast<double*>(dmaParamBLeft.addr) + taskInfo.kPad * taskInfo.n0Pad,
              2.0);
    std::fill(static_cast<double*>(dmaParamBRight.addr),
              static_cast<double*>(dmaParamBRight.addr) + taskInfo.kPad * taskInfo.n1Pad,
              2.0);

    manager->alignBufferInput(taskId);

    // Step through the task
    EXPECT_TRUE(manager->stepAllTask());
    TaskStatus status;
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Calculate0);

    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Calculate1);

    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Finished);

    // Check the output buffers
    MatrixDmaParam dmaParamC00 = manager->getTaskMatrixDmaParamOutputC(taskId, 0);
    MatrixDmaParam dmaParamC01 = manager->getTaskMatrixDmaParamOutputC(taskId, 1);
    MatrixDmaParam dmaParamC10 = manager->getTaskMatrixDmaParamOutputC(taskId, 2);
    MatrixDmaParam dmaParamC11 = manager->getTaskMatrixDmaParamOutputC(taskId, 3);
    double *outputC00 = static_cast<double*>(dmaParamC00.addr);
    double *outputC01 = static_cast<double*>(dmaParamC01.addr);
    double *outputC10 = static_cast<double*>(dmaParamC10.addr);
    double *outputC11 = static_cast<double*>(dmaParamC11.addr);
    for (int i = 0; i < taskInfo.m0; ++i) {
        for (int j = 0; j < taskInfo.n0; ++j) {
            EXPECT_DOUBLE_EQ(outputC00[i * taskInfo.n0Pad + j], 128.0); // m0 * n0
        }
    }
    for (int i = 0; i < taskInfo.m0; ++i) {
        for (int j = 0; j < taskInfo.n1; ++j) {
            EXPECT_DOUBLE_EQ(outputC01[i * taskInfo.n1Pad + j], 128.0); // m0 * n1
        }
    }
    for (int i = 0; i < taskInfo.m1; ++i) {
        for (int j = 0; j < taskInfo.n0; ++j) {
            EXPECT_DOUBLE_EQ(outputC10[i * taskInfo.n0Pad + j], 128.0); // m1 * n0
        }
    }
    for (int i = 0; i < taskInfo.m1; ++i) {
        for (int j = 0; j < taskInfo.n1; ++j) {
            EXPECT_DOUBLE_EQ(outputC11[i * taskInfo.n1Pad + j], 128.0); // m1 * n1
        }
    }    

    // End the task
    EXPECT_TRUE(manager->endTask(taskId));
    EXPECT_TRUE(manager->getTaskStatus(taskId, status));
    EXPECT_EQ(status, TaskStatus::Idle);
}

TEST_F(LocalDgemmManagerTest, AccOnce){
    unsigned int m0 = 121, n0 = 121, k0 = 268;
    unsigned int m1 = m0, n1 = n0, k1 = 136, k1Tail = k0 - k1;

    unsigned int taskId0;
    EXPECT_TRUE(manager->registerTask(m1, n1, k1, false, taskId0));
    LocalDgemmTaskInfo taskInfo0;
    EXPECT_TRUE(manager->getTaskInfo(taskId0, taskInfo0));

    MatrixDmaParam dmaParamAUp0 = manager->getTaskMatrixDmaParamInputA(taskId0, 0);
    MatrixDmaParam dmaParamADown0 = manager->getTaskMatrixDmaParamInputA(taskId0, 1);
    MatrixDmaParam dmaParamBLeft0 = manager->getTaskMatrixDmaParamInputB(taskId0, 0);
    MatrixDmaParam dmaParamBRight0 = manager->getTaskMatrixDmaParamInputB(taskId0, 1);
    std::fill(static_cast<double*>(dmaParamAUp0.addr),
              static_cast<double*>(dmaParamAUp0.addr) + taskInfo0.m0Pad * taskInfo0.kPad,
              1.0);
    std::fill(static_cast<double*>(dmaParamADown0.addr),
              static_cast<double*>(dmaParamADown0.addr) + taskInfo0.m1Pad * taskInfo0.kPad,
              1.0);
    std::fill(static_cast<double*>(dmaParamBLeft0.addr),
              static_cast<double*>(dmaParamBLeft0.addr) + taskInfo0.kPad * taskInfo0.n0Pad,
              2.0);
    std::fill(static_cast<double*>(dmaParamBRight0.addr),
              static_cast<double*>(dmaParamBRight0.addr) + taskInfo0.kPad * taskInfo0.n1Pad,
              2.0);
    manager->alignBufferInput(taskId0);
    EXPECT_TRUE(manager->stepAllTask());

    unsigned int taskId1;
    EXPECT_TRUE(manager->registerTask(m1, n1, k1Tail, true, taskId1));
    LocalDgemmTaskInfo taskInfo1;
    EXPECT_TRUE(manager->getTaskInfo(taskId1, taskInfo1));
    MatrixDmaParam dmaParamAUp1 = manager->getTaskMatrixDmaParamInputA(taskId1, 0);
    MatrixDmaParam dmaParamADown1 = manager->getTaskMatrixDmaParamInputA(taskId1, 1);
    MatrixDmaParam dmaParamBLeft1 = manager->getTaskMatrixDmaParamInputB(taskId1, 0);
    MatrixDmaParam dmaParamBRight1 = manager->getTaskMatrixDmaParamInputB(taskId1, 1);
    std::fill(static_cast<double*>(dmaParamAUp1.addr),
              static_cast<double*>(dmaParamAUp1.addr) + taskInfo1.m0Pad * taskInfo1.kPad,
              1.0);
    std::fill(static_cast<double*>(dmaParamADown1.addr),
              static_cast<double*>(dmaParamADown1.addr) + taskInfo1.m1Pad * taskInfo1.kPad,
              1.0);
    std::fill(static_cast<double*>(dmaParamBLeft1.addr),
              static_cast<double*>(dmaParamBLeft1.addr) + taskInfo1.kPad * taskInfo1.n0Pad,
              2.0);
    std::fill(static_cast<double*>(dmaParamBRight1.addr),
              static_cast<double*>(dmaParamBRight1.addr) + taskInfo1.kPad * taskInfo1.n1Pad,
              2.0);
    manager->alignBufferInput(taskId1);
    EXPECT_TRUE(manager->stepAllTask());

    TaskStatus status0;
    EXPECT_TRUE(manager->getTaskStatus(taskId0, status0));
    EXPECT_EQ(status0, TaskStatus::Calculate1);
    TaskStatus status1;
    EXPECT_TRUE(manager->getTaskStatus(taskId1, status1));
    EXPECT_EQ(status1, TaskStatus::Calculate0);

    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId0, status0));
    EXPECT_EQ(status0, TaskStatus::Finished);
    EXPECT_TRUE(manager->getTaskStatus(taskId1, status1));
    EXPECT_EQ(status1, TaskStatus::Calculate1);

    EXPECT_TRUE(manager->endTask(taskId0));
    EXPECT_TRUE(manager->stepAllTask());
    EXPECT_TRUE(manager->getTaskStatus(taskId1, status1));
    EXPECT_EQ(status1, TaskStatus::Finished);

    // Check the output buffers
    MatrixDmaParam dmaParamC00 = manager->getTaskMatrixDmaParamOutputC(taskId1, 0);
    MatrixDmaParam dmaParamC01 = manager->getTaskMatrixDmaParamOutputC(taskId1, 1);
    MatrixDmaParam dmaParamC10 = manager->getTaskMatrixDmaParamOutputC(taskId1, 2);
    MatrixDmaParam dmaParamC11 = manager->getTaskMatrixDmaParamOutputC(taskId1, 3);
    double *outputC00 = static_cast<double*>(dmaParamC00.addr);
    double *outputC01 = static_cast<double*>(dmaParamC01.addr);
    double *outputC10 = static_cast<double*>(dmaParamC10.addr);
    double *outputC11 = static_cast<double*>(dmaParamC11.addr);
    for (int i = 0; i < taskInfo1.m0; ++i) {
        for (int j = 0; j < taskInfo1.n0; ++j) {
            EXPECT_DOUBLE_EQ(outputC00[i * taskInfo1.n0Pad + j], 536.0); // m0 * n0
        }
    }
    for (int i = 0; i < taskInfo1.m0; ++i) {
        for (int j = 0; j < taskInfo1.n1; ++j) {
            EXPECT_DOUBLE_EQ(outputC01[i * taskInfo1.n1Pad + j], 536.0); // m0 * n1
        }
    }
    for (int i = 0; i < taskInfo1.m1; ++i) {
        for (int j = 0; j < taskInfo1.n0; ++j) {
            EXPECT_DOUBLE_EQ(outputC10[i * taskInfo1.n0Pad + j], 536.0); // m1 * n0
        }
    }
    for (int i = 0; i < taskInfo1.m1; ++i) {
        for (int j = 0; j < taskInfo1.n1; ++j) {
            EXPECT_DOUBLE_EQ(outputC11[i * taskInfo1.n1Pad + j], 536.0); // m1 * n1
        }
    }
    // End the task
    EXPECT_TRUE(manager->endTask(taskId1));
    EXPECT_TRUE(manager->getTaskStatus(taskId1, status1));
    EXPECT_EQ(status1, TaskStatus::Idle);
}