#include <Arduino.h>
#include <unity.h>
#include <SPIFFS.h>
#include "../src/storage/storage_base.h"

using namespace one::storage;

void setUp(void) {
    // Initialize SPIFFS with format on failure
    if(!SPIFFS.begin(true)) {
        TEST_FAIL_MESSAGE("SPIFFS Mount Failed");
    }
}

void tearDown(void) {
    closeStorage();
    SPIFFS.format(); // Clean up after tests
}

void test_init_storage() {
    // Test initialization
    TEST_ASSERT_TRUE(initStorage("test_instance", true));
    TEST_ASSERT_TRUE(doesStorageExist("test_instance"));

    // Verify directory structure
    TEST_ASSERT_TRUE(SPIFFS.exists("/one/test_instance/objects"));
    TEST_ASSERT_TRUE(SPIFFS.exists("/one/test_instance/tmp"));
    TEST_ASSERT_TRUE(SPIFFS.exists("/one/test_instance/rmaps"));
    TEST_ASSERT_TRUE(SPIFFS.exists("/one/test_instance/vmaps"));
    TEST_ASSERT_TRUE(SPIFFS.exists("/one/test_instance/acache"));
    TEST_ASSERT_TRUE(SPIFFS.exists("/one/test_instance/private"));
}

void test_file_operations() {
    // Initialize storage
    TEST_ASSERT_TRUE(initStorage("test_instance", true));

    // Test writing
    const char* testContent = "Hello ONE!";
    StorageResult writeResult = writeUTF8TextFile("test.txt", testContent);
    TEST_ASSERT_TRUE(writeResult.success);
    TEST_ASSERT_EQUAL(StorageError::NONE, writeResult.error);

    // Test existence
    TEST_ASSERT_TRUE(exists("test.txt"));

    // Test reading
    String content;
    StorageResult readResult = readUTF8TextFile("test.txt", content);
    TEST_ASSERT_TRUE(readResult.success);
    TEST_ASSERT_EQUAL(StorageError::NONE, readResult.error);
    TEST_ASSERT_EQUAL_STRING(testContent, content.c_str());

    // Test file size
    TEST_ASSERT_EQUAL(strlen(testContent), fileSize("test.txt"));
}

void test_error_handling() {
    // Initialize storage
    TEST_ASSERT_TRUE(initStorage("test_instance", true));

    // Test reading non-existent file
    String content;
    StorageResult result = readUTF8TextFile("nonexistent.txt", content);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL(StorageError::FILE_NOT_FOUND, result.error);

    // Test operations without initialization
    closeStorage();
    result = readUTF8TextFile("test.txt", content);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL(StorageError::IO_ERROR, result.error);
}

void test_path_management() {
    // Initialize storage
    TEST_ASSERT_TRUE(initStorage("test_instance", true));

    // Test path normalization for different storage types
    String objectPath = normalizeFilename("test.txt", StorageDir::OBJECTS);
    TEST_ASSERT_EQUAL_STRING("/one/test_instance/objects/test.txt", objectPath.c_str());

    String tmpPath = normalizeFilename("temp.txt", StorageDir::TMP);
    TEST_ASSERT_EQUAL_STRING("/one/test_instance/tmp/temp.txt", tmpPath.c_str());

    // Test getting storage directory
    String objectsDir = getStorageDirForFileType(StorageDir::OBJECTS);
    TEST_ASSERT_EQUAL_STRING("/one/test_instance/objects", objectsDir.c_str());
}

void test_storage_persistence() {
    // Create initial storage
    TEST_ASSERT_TRUE(initStorage("test_instance", false));
    
    // Write a test file
    const char* testContent = "Persistent Content";
    StorageResult writeResult = writeUTF8TextFile("persistent.txt", testContent);
    TEST_ASSERT_TRUE(writeResult.success);

    // Close and reopen storage
    closeStorage();
    TEST_ASSERT_TRUE(initStorage("test_instance", false));

    // Verify file still exists
    TEST_ASSERT_TRUE(exists("persistent.txt"));
    
    // Verify content
    String content;
    StorageResult readResult = readUTF8TextFile("persistent.txt", content);
    TEST_ASSERT_TRUE(readResult.success);
    TEST_ASSERT_EQUAL_STRING(testContent, content.c_str());
}

void setup() {
    delay(2000); // Allow board to settle
    UNITY_BEGIN();
    
    RUN_TEST(test_init_storage);
    RUN_TEST(test_file_operations);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_path_management);
    RUN_TEST(test_storage_persistence);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
} 