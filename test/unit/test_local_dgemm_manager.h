#ifndef TEST_LOCAL_DGEMM_MANAGER_H
#define TEST_LOCAL_DGEMM_MANAGER_H

#include <gtest/gtest.h>
#include "local_dgemm_manager.h"

class LocalDgemmManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the LocalDgemmManager instance
        manager = &LocalDgemmManager::getInstance();
    }

    void TearDown() override {
        // Reset the LocalDgemmManager instance
        manager->reset();
    }

    LocalDgemmManager* manager;
};

#endif // TEST_LOCAL_DGEMM_MANAGER_H