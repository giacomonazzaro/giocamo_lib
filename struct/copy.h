#pragma once
#include <cstring>

#include "basic/allocator.h"
#include "basic/array.h"
#include "basic/memory.h"
#include "struct/print.h"

template <typename T>
inline void copy_struct(T& w, const T& v, Allocator* allocator) {
    w = v;
}

template <typename T>
inline T& copy(const T& x, Allocator* allocator) {
    auto& output = allocator->allocate<T>();
    copy_struct(output, x, allocator);
    return output;
}

template <typename T>
inline void copy_struct(array<T>& w, const array<T>& v, Allocator* allocator) {
    w.count = v.size();
    if (v.size() == 0) {
        w.data = nullptr;
        return;
    }
    w.data = (T*)allocator->allocate_bytes(sizeof(T) * v.count);

    for (size_t i = 0; i < v.size(); i++) {
        copy_struct(w[i], v[i], allocator);
    }
}

#define COPY_BY_MEMBER(T)                                                    \
    inline void copy_struct(T& result, const T& t, Allocator* allocator) {   \
        visit_struct::for_each(t, [&](const char* name, const auto& value) { \
            using MemberType    = std::decay_t<decltype(value)>;             \
            auto  member_offset = (byte*)&value - (byte*)&t;                 \
            auto& dest_member   = *(MemberType*)((byte*)&result +            \
                                               member_offset);             \
            copy_struct(dest_member, value, allocator);                      \
        });                                                                  \
    }
