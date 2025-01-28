#include <Arduino.h>
#include <unity.h>
#include <SPIFFS.h>
#include "../src/storage/storage_base.h"
#include "../src/storage/storage_streams.h"
#include "../src/storage/filesystem.h"

using namespace one::storage;

std::unique_ptr<IFileSystem> fs;

void setUp(void) {
    if(!SPIFFS.begin(true)) {
        TEST_FAIL_MESSAGE("SPIFFS Mount Failed");
    }
    TEST_ASSERT_TRUE(initStorage("test_instance", true));
    fs = std::make_unique<ESPFileSystem>();
}

void tearDown(void) {
    fs.reset();
    closeStorage();
    SPIFFS.format();
}

void test_layered_file_operations() {
    // Layer 1: Basic Storage
    const char* testData = "Test data for layered operations";
    StorageResult writeResult = writeUTF8TextFile("direct.txt", testData);
    TEST_ASSERT_TRUE(writeResult.success);
    
    String content;
    StorageResult readResult = readUTF8TextFile("direct.txt", content);
    TEST_ASSERT_TRUE(readResult.success);
    TEST_ASSERT_EQUAL_STRING(testData, content.c_str());

    // Layer 2: Streams
    auto writeStream = streams::createFileWriteStream("stream.txt");
    TEST_ASSERT_NOT_NULL(writeStream);
    size_t written = writeStream->write((const uint8_t*)testData, strlen(testData));
    TEST_ASSERT_EQUAL(strlen(testData), written);
    writeStream->close();
    delete writeStream;

    auto readStream = streams::createFileReadStream("stream.txt");
    TEST_ASSERT_NOT_NULL(readStream);
    uint8_t buffer[64] = {0};
    size_t bytesRead = readStream->read(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(strlen(testData), bytesRead);
    TEST_ASSERT_EQUAL_STRING(testData, (char*)buffer);
    readStream->close();
    delete readStream;

    // Layer 3: Filesystem
    TEST_ASSERT_TRUE(fs->writeFile("/fs.txt", (const uint8_t*)testData, strlen(testData)));
    FileContent fsContent = fs->readFile("/fs.txt");
    TEST_ASSERT_NOT_NULL(fsContent.data.get());
    TEST_ASSERT_EQUAL(strlen(testData), fsContent.size);
    TEST_ASSERT_EQUAL_MEMORY(testData, fsContent.data.get(), fsContent.size);
}

void test_cross_layer_operations() {
    // Write with filesystem, read with streams
    const char* testData1 = "Data written by filesystem";
    TEST_ASSERT_TRUE(fs->writeFile("/test1.txt", (const uint8_t*)testData1, strlen(testData1)));
    
    auto readStream = streams::createFileReadStream("test1.txt");
    TEST_ASSERT_NOT_NULL(readStream);
    uint8_t buffer1[64] = {0};
    size_t bytesRead1 = readStream->read(buffer1, sizeof(buffer1));
    TEST_ASSERT_EQUAL(strlen(testData1), bytesRead1);
    TEST_ASSERT_EQUAL_STRING(testData1, (char*)buffer1);
    readStream->close();
    delete readStream;

    // Write with streams, read with base storage
    const char* testData2 = "Data written by stream";
    auto writeStream = streams::createFileWriteStream("test2.txt");
    TEST_ASSERT_NOT_NULL(writeStream);
    writeStream->write((const uint8_t*)testData2, strlen(testData2));
    writeStream->close();
    delete writeStream;

    String content;
    StorageResult readResult = readUTF8TextFile("test2.txt", content);
    TEST_ASSERT_TRUE(readResult.success);
    TEST_ASSERT_EQUAL_STRING(testData2, content.c_str());

    // Write with base storage, read with filesystem
    const char* testData3 = "Data written by base storage";
    StorageResult writeResult = writeUTF8TextFile("test3.txt", testData3);
    TEST_ASSERT_TRUE(writeResult.success);

    FileContent fsContent = fs->readFile("/test3.txt");
    TEST_ASSERT_NOT_NULL(fsContent.data.get());
    TEST_ASSERT_EQUAL(strlen(testData3), fsContent.size);
    TEST_ASSERT_EQUAL_MEMORY(testData3, fsContent.data.get(), fsContent.size);
}

void test_directory_hierarchy() {
    // Create directory structure using filesystem
    TEST_ASSERT_TRUE(fs->createDir("/root"));
    TEST_ASSERT_TRUE(fs->createDir("/root/dir1"));
    TEST_ASSERT_TRUE(fs->createDir("/root/dir2"));

    // Write files using different layers
    const char* data1 = "File in dir1";
    const char* data2 = "File in dir2";
    
    // Write using base storage
    StorageResult writeResult = writeUTF8TextFile("/root/dir1/base.txt", data1);
    TEST_ASSERT_TRUE(writeResult.success);

    // Write using streams
    auto writeStream = streams::createFileWriteStream("/root/dir1/stream.txt");
    TEST_ASSERT_NOT_NULL(writeStream);
    writeStream->write((const uint8_t*)data1, strlen(data1));
    writeStream->close();
    delete writeStream;

    // Write using filesystem
    TEST_ASSERT_TRUE(fs->writeFile("/root/dir2/fs.txt", (const uint8_t*)data2, strlen(data2)));

    // Verify directory structure
    std::vector<DirectoryEntry> rootEntries = fs->readDir("/root");
    TEST_ASSERT_EQUAL(2, rootEntries.size());

    std::vector<DirectoryEntry> dir1Entries = fs->readDir("/root/dir1");
    TEST_ASSERT_EQUAL(2, dir1Entries.size());

    std::vector<DirectoryEntry> dir2Entries = fs->readDir("/root/dir2");
    TEST_ASSERT_EQUAL(1, dir2Entries.size());

    // Verify file contents using different layers
    String content1;
    StorageResult readResult = readUTF8TextFile("/root/dir1/base.txt", content1);
    TEST_ASSERT_TRUE(readResult.success);
    TEST_ASSERT_EQUAL_STRING(data1, content1.c_str());

    auto readStream = streams::createFileReadStream("/root/dir1/stream.txt");
    TEST_ASSERT_NOT_NULL(readStream);
    uint8_t buffer[64] = {0};
    size_t bytesRead = readStream->read(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(strlen(data1), bytesRead);
    TEST_ASSERT_EQUAL_STRING(data1, (char*)buffer);
    readStream->close();
    delete readStream;

    FileContent fsContent = fs->readFile("/root/dir2/fs.txt");
    TEST_ASSERT_NOT_NULL(fsContent.data.get());
    TEST_ASSERT_EQUAL(strlen(data2), fsContent.size);
    TEST_ASSERT_EQUAL_MEMORY(data2, fsContent.data.get(), fsContent.size);
}

void setup() {
    delay(2000); // Allow board to settle
    UNITY_BEGIN();
    
    RUN_TEST(test_layered_file_operations);
    RUN_TEST(test_cross_layer_operations);
    RUN_TEST(test_directory_hierarchy);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
} 