#pragma once
#include <cassert>
#include <string>
#include <vector>

// Serializer holds the information needed to serialize data expressed in binary
// format into/from file. To minimize disk access, a memory buffer is used to
// store temporary result.

struct Serializer {
    FILE* file    = nullptr;
    int   version = 0;
    bool  is_writer;

    unsigned char* buffer          = nullptr;
    size_t         buffer_capacity = 0;
    size_t         buffer_count    = 0;

    ~Serializer() {
        if (file or buffer) assert(0 && "Close serializer before destruction!");
    }
};

inline Serializer make_serializer(const std::string& filename, bool save,
                                  size_t buffer_capacity) {
    Serializer srl;
    srl.is_writer = save;
    srl.file      = fopen(filename.c_str(), save ? "w+" : "r");
    if (not srl.file) {
        printf("SERIALIZER ERROR: could not open file %s\n\n",
               filename.c_str());
        assert(0);
    }

    srl.buffer_capacity = buffer_capacity;
    if (srl.buffer_capacity > 0) {
        srl.buffer = new unsigned char[buffer_capacity];
        if (not srl.buffer) {
            printf("SERIALIZER ERROR: could not allocate buffer for file %s\n\n",
                   filename.c_str());
            assert(0);
        }
    } else
        srl.buffer = nullptr;

    if (not srl.is_writer and srl.buffer_capacity > 0)
        fread(srl.buffer, srl.buffer_capacity, 1, srl.file);

    return srl;
}

inline Serializer make_reader(const std::string& filename,
                              size_t             buffer_capacity) {
    return make_serializer(filename, false, buffer_capacity);
}

inline Serializer make_writer(const std::string& filename,
                              size_t             buffer_capacity) {
    return make_serializer(filename, true, buffer_capacity);
}

// Release resources.
void close_serializer(Serializer& srl) {
    if (srl.buffer) {
        if (srl.is_writer) fwrite(srl.buffer, srl.buffer_count, 1, srl.file);

        delete[] srl.buffer;
        srl.buffer          = nullptr;
        srl.buffer_capacity = 0;
        srl.buffer_count    = 0;
    }
    if (srl.file) fclose(srl.file);
    srl.file = nullptr;
}

// Write/read from/to memory buffer.
inline void buffer_serialize(Serializer& srl, void* data, size_t size) {
    if (srl.buffer_capacity == 0) return;
    if (size == 0) return;
    assert(size <= srl.buffer_capacity - srl.buffer_count);
    assert(data);

    // void* memcpy(void* destination, const void* source, size_t num);
    if (srl.is_writer)
        memcpy(srl.buffer + srl.buffer_count, data, size);
    else
        memcpy(data, srl.buffer + srl.buffer_count, size);

    srl.buffer_count += size;
}

// Read using buffer when possible.
inline void read(Serializer& srl, void* data, size_t size) {
    // assert(size > 0);
    assert(data);
    if (size == 0) return;

    // Complete current buffer if needed.
    if (size >= srl.buffer_capacity - srl.buffer_count) {
        auto count = srl.buffer_capacity - srl.buffer_count;
        buffer_serialize(srl, data, count);
        data = (void*)((unsigned char*)data + count);
        size -= count;

        // If the rest is too big, don't use buffer
        if (size >= srl.buffer_capacity) {
            if (srl.is_writer)
                fwrite(data, size, 1, srl.file);
            else
                fread(data, size, 1, srl.file);
            size = 0;
        }

        // Refill buffer.
        assert(srl.buffer_count == srl.buffer_capacity);
        fread(srl.buffer, srl.buffer_capacity, 1, srl.file);
        srl.buffer_count = 0;
    }

    buffer_serialize(srl, data, size);
}

// Write using buffer when possible.
inline void write(Serializer& srl, void* data, size_t size) {
    // assert(size >
    assert(data);
    if (size == 0) return;

    if (size >= srl.buffer_capacity - srl.buffer_count) {
        fwrite(srl.buffer, srl.buffer_count, 1, srl.file);
        fwrite(data, size, 1, srl.file);
        srl.buffer_count = 0;
    } else {
        buffer_serialize(srl, data, size);
    }
}

// Serialize (write or read) struct with no allocated resource
template <typename Type>
inline void serialize(Serializer& srl, Type& data) {
    if (srl.is_writer)
        write(srl, &data, sizeof(Type));
    else
        read(srl, &data, sizeof(Type));
}

template <typename Type>
inline void save_to_file(const std::string& filename, const Type& object,
                         int buffer_size = 1048576) {
    auto writer = make_writer(filename, buffer_size);
    serialize(writer, *(Type*)&object);
    close_serializer(writer);
}

template <typename Type>
inline void load_from_file(const std::string& filename, Type& object,
                           int buffer_size = 1048576) {
    auto reader = make_reader(filename, buffer_size);
    serialize(reader, object);
    close_serializer(reader);
}

template <typename Type>
inline Type make_from_file(const std::string& filename,
                           int                buffer_size = 1048576) {
    Type object;
    auto reader = make_reader(filename, 0);
    serialize(reader, object);
    close_serializer(reader);
    return object;
}
