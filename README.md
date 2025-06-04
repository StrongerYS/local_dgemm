# APC Local DGEMM

本项目实现了一个本地高性能分块矩阵乘法（DGEMM）任务管理器 `LocalDgemmManager`，支持多任务、分块、累加、资源池管理等特性，适用于高性能和嵌入式场景。

## 设计理念

- **任务驱动**：用户通过注册任务，获得输入/输出 buffer 地址和排布信息。
- **数据自主**：用户按 manager 提供的排布说明填充输入数据。
- **状态机推进**：只有用户显式标记数据已准备好后，manager 才会调度计算。
- **资源池管理**：任务和 buffer 资源池，未释放资源时新任务注册会失败。
- **累加支持**：支持多步累加（accumulate）场景，适合大矩阵分块计算。
- **手动释放**：用户决定何时取出结果、何时释放任务资源。
- **并发调度**：内部支持多线程并发计算，自动分配空闲线程。

## 典型使用流程

1. **注册任务**  
   ```cpp
   unsigned int taskId;
   EXPECT_TRUE(manager->registerTask(m, n, k, acc, taskId));
   // manager 内部分配资源，返回 taskId
   ```
2. **获取 buffer 地址与排布**  
   ```cpp
   MatrixDmaParam dmaParamAUp = manager->getTaskMatrixDmaParamInputA(taskId, 0);
   // 其他 buffer 同理
   ```
   用户根据返回的地址和排布信息填充数据。
3. **数据对齐**  
   ```cpp
   manager->alignBufferInput(taskId);
   ```
4. **标记数据已准备好**  
   ```cpp
   manager->markTaskDataReady(taskId);
   ```
5. **推进任务计算**  
   ```cpp
   // 推荐批量注册任务后统一推进
   while (!所有任务都完成) {
       manager->stepAllTask();
   }
   ```
6. **获取输出数据**  
   ```cpp
   MatrixDmaParam dmaParamC00 = manager->getTaskMatrixDmaParamOutputC(taskId, 0);
   // 用户根据地址取出结果
   ```
7. **释放任务资源**  
   ```cpp
   manager->endTask(taskId);
   ```

## 任务状态流转

- Idle → Ready → DataReady → Calculate0 → Calculate1 → Finished → Idle
- 只有 DataReady 状态的任务才会被调度执行。

## 并发与资源池

- manager 内部最多支持固定数量的任务并发（由 `NumTasks` 决定），每个任务分配独立 buffer。
- 若资源未释放，新的任务注册会失败。
- 多线程自动分配空闲线程执行计算。

## 累加（accumulate）用例说明

- 支持大矩阵分多步累加计算，后续任务可设置 `acc=true`，在前一次结果基础上继续累加。
- 典型用例见 `test/unit/TEST_local_dgemm_manager.cpp` 的 `AccMultiple` 测试。

## 主要接口

- `registerTask(m, n, k, acc, taskId)`：注册任务，分配资源，返回 taskId。
- `getTaskMatrixDmaParamInputA/B(taskId, idx)`：获取输入 buffer 地址和排布。
- `alignBufferInput(taskId)`：对输入 buffer 进行补零对齐。
- `markTaskDataReady(taskId)`：标记数据已准备好。
- `stepAllTask()`：推进所有可执行任务。
- `getTaskMatrixDmaParamOutputC(taskId, idx)`：获取输出 buffer 地址。
- `endTask(taskId)`：释放任务资源。
- `getTaskStatus(taskId, status)`：查询任务状态。

## 编译与测试

```sh
cd build
./build.sh
ctest --output-on-failure
```

## 参考

- 测试用例：`test/unit/TEST_local_dgemm_manager.cpp`
- 接口头文件：`include/local_dgemm_manager.h`

