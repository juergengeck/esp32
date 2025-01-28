#include <Arduino.h>
#include <unity.h>
#include <SPIFFS.h>
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

void test_file_operations() {
    // Write file
    const char* testData = "Hello ONE Filesystem!";
    TEST_ASSERT_TRUE(fs->writeFile("/test.txt", (const uint8_t*)testData, strlen(testData)));

    // Check existence
    TEST_ASSERT_TRUE(fs->exists("/test.txt"));

    // Read file
    FileContent content = fs->readFile("/test.txt");
    TEST_ASSERT_NOT_NULL(content.data.get());
    TEST_ASSERT_EQUAL(strlen(testData), content.size);
    TEST_ASSERT_EQUAL_MEMORY(testData, content.data.get(), content.size);

    // Get file info
    FileDescription desc = fs->stat("/test.txt");
    TEST_ASSERT_EQUAL(strlen(testData), desc.size);
    TEST_ASSERT_FALSE(desc.isDirectory);
    TEST_ASSERT_FALSE(desc.isSymlink);

    // Append to file
    const char* appendData = " Appended content.";
    TEST_ASSERT_TRUE(fs->appendFile("/test.txt", (const uint8_t*)appendData, strlen(appendData)));

    // Read appended content
    content = fs->readFile("/test.txt");
    TEST_ASSERT_NOT_NULL(content.data.get());
    TEST_ASSERT_EQUAL(strlen(testData) + strlen(appendData), content.size);

    // Remove file
    TEST_ASSERT_TRUE(fs->removeFile("/test.txt"));
    TEST_ASSERT_FALSE(fs->exists("/test.txt"));
}

void test_directory_operations() {
    // Create directory
    TEST_ASSERT_TRUE(fs->createDir("/testdir"));
    TEST_ASSERT_TRUE(fs->exists("/testdir"));

    // Create files in directory
    TEST_ASSERT_TRUE(fs->writeFile("/testdir/file1.txt", (const uint8_t*)"File 1", 6));
    TEST_ASSERT_TRUE(fs->writeFile("/testdir/file2.txt", (const uint8_t*)"File 2", 6));

    // Read directory
    std::vector<DirectoryEntry> entries = fs->readDir("/testdir");
    TEST_ASSERT_EQUAL(2, entries.size());

    // Verify directory entries
    bool foundFile1 = false, foundFile2 = false;
    for (const auto& entry : entries) {
        if (entry.name == "file1.txt") {
            foundFile1 = true;
            TEST_ASSERT_EQUAL(6, entry.desc.size);
        } else if (entry.name == "file2.txt") {
            foundFile2 = true;
            TEST_ASSERT_EQUAL(6, entry.desc.size);
        }
    }
    TEST_ASSERT_TRUE(foundFile1);
    TEST_ASSERT_TRUE(foundFile2);

    // Remove directory
    TEST_ASSERT_TRUE(fs->removeDir("/testdir"));
    TEST_ASSERT_FALSE(fs->exists("/testdir"));
}

void test_chunked_operations() {
    // Create large test file
    const size_t testSize = 8192;
    std::unique_ptr<uint8_t[]> testData(new uint8_t[testSize]);
    for(size_t i = 0; i < testSize; i++) {
        testData[i] = i & 0xFF;
    }

    TEST_ASSERT_TRUE(fs->writeFile("/large.bin", testData.get(), testSize));

    // Read in chunks
    const size_t chunkSize = 1024;
    for(size_t offset = 0; offset < testSize; offset += chunkSize) {
        FileContent chunk = fs->readFileChunk("/large.bin", offset, chunkSize);
        TEST_ASSERT_NOT_NULL(chunk.data.get());
        TEST_ASSERT_EQUAL_MEMORY(testData.get() + offset, chunk.data.get(), chunk.size);
    }

    // Test truncate
    const size_t newSize = 4096;
    TEST_ASSERT_TRUE(fs->truncate("/large.bin", newSize));
    
    FileDescription desc = fs->stat("/large.bin");
    TEST_ASSERT_EQUAL(newSize, desc.size);
}

void test_stream_operations() {
    // Write using stream
    auto writeStream = fs->createWriteStream("/stream.txt");
    TEST_ASSERT_NOT_NULL(writeStream.get());

    const char* testData = "Stream test data";
    size_t written = writeStream->write((const uint8_t*)testData, strlen(testData));
    TEST_ASSERT_EQUAL(strlen(testData), written);
    writeStream->close();

    // Read using stream
    auto readStream = fs->createReadStream("/stream.txt");
    TEST_ASSERT_NOT_NULL(readStream.get());

    uint8_t buffer[64] = {0};
    size_t bytesRead = readStream->read(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(strlen(testData), bytesRead);
    TEST_ASSERT_EQUAL_MEMORY(testData, buffer, bytesRead);
    readStream->close();
}

void test_space_info() {
    size_t total = fs->totalSpace();
    size_t used = fs->usedSpace();
    size_t free = fs->freeSpace();

    TEST_ASSERT_TRUE(total > 0);
    TEST_ASSERT_EQUAL(total, used + free);

    // Write some data and check space changes
    const char* testData = "Space test data";
    TEST_ASSERT_TRUE(fs->writeFile("/space.txt", (const uint8_t*)testData, strlen(testData)));

    size_t newUsed = fs->usedSpace();
    TEST_ASSERT_TRUE(newUsed >= used);
}

void setup() {
    delay(2000); // Allow board to settle
    UNITY_BEGIN();
    
    RUN_TEST(test_file_operations);
    RUN_TEST(test_directory_operations);
    RUN_TEST(test_chunked_operations);
    RUN_TEST(test_stream_operations);
    RUN_TEST(test_space_info);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
} 