#include <flipper.h>
#include <ble.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Adjust for file size limits, but anything above this isn't guaranteed to work
#define MAX_FILE_SIZE 1024 * 1024

static uint8_t file_buffer[MAX_FILE_SIZE];      // Buffer stores file contnet
static size_t file_size = 0;                    // Transferred file size
static uint16_t conn_id = 0;

// [SENDER MODE] Read file into buffer
int read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return -1;
    }

    file_size = fread(file_buffer, 1, MAX_FILE_SIZE, file);
    fclose(file);

    if (file_size == 0) {
        printf("Error: File is empty or not found.\n");
        return -1;
    }

    printf("File read successfully, size: %zu bytes\n", file_size);
    return 0;
}

// [SENDER MODE] Send file
void send_file_data(uint16_t conn_id) {
    for (size_t i = 0; i < file_size; i += 20) {  // Currently sends 20 bytes at a time, adjust if necessary
        size_t chunk_size = (i + 20 <= file_size) ? 20 : (file_size - i);
        ble_send_data(conn_id, file_buffer + i, chunk_size);
        printf("Sent %zu bytes.\n", chunk_size);
        delay(100); // Keep this to ensure data is sent and received properly
    }
    printf("File transfer complete.\n");
}

// [RECEIVER MODE] Callback to receive data
void ble_data_received(uint8_t *data, uint16_t length) {
    static size_t received_size = 0;

    if (received_size + length > MAX_FILE_SIZE) {
        printf("Error: File size exceeds limit.\n");
        return;
    }

    // Move received data into buffer
    memcpy(file_buffer + received_size, data, length);
    received_size += length;

    // When whole thing is received, move it to file system, change file name appropriately
    if (received_size == file_size) {
        FILE *file = fopen("/shared/received_file.bin", "wb");
        fwrite(file_buffer, 1, received_size, file);
        fclose(file);
        printf("File received and saved successfully!\n");
    }
}

// [RECEIVER MODE] Bluetooth connection callback
void ble_connected_callback(uint16_t conn_id) {
    printf("Receiver connected via Bluetooth!\n");
}

// Bluetooth service setup
void ble_setup_service() {
    ble_register_service(0x180D);
    ble_register_characteristic(0x2A37, BLE_CHAR_WRITE, ble_data_received);
}

// [RECEIVER MODE] Wait and receive file
void receiver_mode() {
    printf("Receiver mode: Waiting for connection...\n");

    // Initialize bluetooth
    ble_init();
    ble_setup_service();

    // Start advertising (be discoverable to other Flippers)
    ble_start_advertising();

    // This keeps the receiver active
    while (1) {
        ble_process();
    }
}

// [SENDER MODE] Wait for selected file and send
void sender_mode() {
    // Select a file to send, as proof of concept this is what I'm sending
    printf("Sender mode: Selecting file...\n");

    if (read_file("/shared/test_file.bin") != 0) {
        return;
    }

    // Initialize blutetooth and start scanning
    ble_init();
    ble_setup_service();
    ble_start_scanning();

    // Wait for a receiver to connect
    while (1) {
        ble_process();
    }
}

// MAIN
int main(void) {
    int mode;

    printf("Select mode:\n1. Sender\n2. Receiver\n> ");
    scanf("%d", &mode);

    if (mode == 1) {
        sender_mode();
    } else if (mode == 2) {
        receiver_mode();
    } else {
        printf("Invalid mode selected.\n");
    }

    return 0;
}
