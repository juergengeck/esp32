#include <Arduino.h>
#include <unity.h>
#include <SPIFFS.h>
#include "../src/storage/storage_streams.h"

using namespace one::storage;

void setUp(void) {
    if(!SPIFFS.begin(true)) {
        TEST_FAIL_MESSAGE("SPIFFS Mount Failed");
    }
    TEST_ASSERT_TRUE(initStorage("test_instance", true));
}

void tearDown(void) {
    closeStorage();
    SPIFFS.format();
}

void test_read_stream() {
    // Create test file
    const char* testContent = "Hello ONE Stream!";
    StorageResult writeResult = writeUTF8TextFile("test_read.txt", testContent);
    TEST_ASSERT_TRUE(writeResult.success);

    // Create read stream
    SimpleReadStream* stream = streams::createFileReadStream("test_read.txt");
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(stream->isOpen());

    // Read data
    uint8_t buffer[64] = {0};
    size_t bytesRead = stream->read(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(strlen(testContent), bytesRead);
    TEST_ASSERT_EQUAL_STRING(testContent, (char*)buffer);

    // Test event handler
    bool eventReceived = false;
    stream->onData([&eventReceived](const uint8_t* data, size_t length) {
        eventReceived = true;
    });

    bytesRead = stream->read(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, bytesRead); // EOF
    
    stream->close();
    delete stream;
}

void test_write_stream() {
    // Create write stream
    SimpleWriteStream* stream = streams::createFileWriteStream("test_write.txt");
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(stream->isOpen());

    // Test data
    const char* testData = "Writing to stream";
    size_t written = stream->write((uint8_t*)testData, strlen(testData));
    TEST_ASSERT_EQUAL(strlen(testData), written);

    // Test drain event
    bool drainCalled = false;
    stream->onDrain([&drainCalled]() {
        drainCalled = true;
    });

    stream->flush();
    stream->close();
    delete stream;

    // Verify written content
    String content;
    StorageResult readResult = readUTF8TextFile("test_write.txt", content);
    TEST_ASSERT_TRUE(readResult.success);
    TEST_ASSERT_EQUAL_STRING(testData, content.c_str());
}

void test_chunked_read_stream() {
    // Create large test file
    const size_t testSize = 8192;
    char* testData = new char[testSize + 1];
    for(size_t i = 0; i < testSize; i++) {
        testData[i] = 'A' + (i % 26);
    }
    testData[testSize] = '\0';
    
    StorageResult writeResult = writeUTF8TextFile("test_chunked.txt", testData);
    TEST_ASSERT_TRUE(writeResult.success);

    // Create chunked read stream with 1KB chunks
    const size_t chunkSize = 1024;
    SimpleReadStream* stream = streams::createChunkedReadStream("test_chunked.txt", chunkSize);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(stream->isOpen());

    // Read and verify chunks
    uint8_t* buffer = new uint8_t[chunkSize];
    size_t totalRead = 0;
    size_t bytesRead;

    while((bytesRead = stream->read(buffer, chunkSize)) > 0) {
        TEST_ASSERT_TRUE(bytesRead <= chunkSize);
        TEST_ASSERT_EQUAL_MEMORY(testData + totalRead, buffer, bytesRead);
        totalRead += bytesRead;
    }

    TEST_ASSERT_EQUAL(testSize, totalRead);

    stream->close();
    delete stream;
    delete[] buffer;
    delete[] testData;
}

void test_stream_error_handling() {
    // Test non-existent file
    SimpleReadStream* readStream = streams::createFileReadStream("nonexistent.txt");
    TEST_ASSERT_NOT_NULL(readStream);
    TEST_ASSERT_FALSE(readStream->isOpen());
    delete readStream;

    // Test invalid write stream
    SimpleWriteStream* writeStream = streams::createFileWriteStream(nullptr);
    TEST_ASSERT_NULL(writeStream);
}

void setup() {
    delay(2000); // Allow board to settle
    UNITY_BEGIN();
    
    RUN_TEST(test_read_stream);
    RUN_TEST(test_write_stream);
    RUN_TEST(test_chunked_read_stream);
    RUN_TEST(test_stream_error_handling);
    
    UNITY_END();
}

void loop() {
    // Nothing to do here
} 