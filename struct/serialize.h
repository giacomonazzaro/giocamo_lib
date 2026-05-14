#pragma once
#include "struct/print.h"
#include "serializer.h"

template <typename T>
struct Custom_Serialization;

template <typename T, typename = void>
struct has_custom_serialization : std::false_type {};

template <typename T>
struct has_custom_serialization<
    T, std::void_t<decltype(sizeof(Custom_Serialization<T>))>>
    : std::true_type {};

// Helper variable template
template <typename T>
inline constexpr bool has_custom_serialization_v =
    has_custom_serialization<T>::value;

template <typename T>
inline void serialize_struct(Serializer& srl, T& t) {
    if constexpr (has_custom_serialization_v<T>) {
        Custom_Serialization<T>::serialize_struct(srl, t);
    } else {
        serialize(srl, t);
    }
}

template <typename T>
struct Custom_Serialization<std::vector<T>> {
    static void serialize_struct(Serializer& srl, std::vector<T>& v) {
        size_t count = v.size();
        serialize(srl, count);
        if (!srl.is_writer) {
            v = std::vector<T>(count);
        }
        if (count == 0) return;
        if constexpr (has_custom_serialization_v<T>) {
            for (size_t i = 0; i < count; i++) {
                ::serialize_struct(srl, v[i]);
            }
        } else if constexpr (visit_struct::traits::is_visitable<
                                 std::decay_t<T>>::value) {
            for (size_t i = 0; i < count; i++) {
                ::serialize_struct(srl, v[i]);
            }
        } else {
            if (srl.is_writer) {
                write(srl, v.data(), sizeof(T) * count);
            } else {
                read(srl, v.data(), sizeof(T) * count);
            }
        }
    }
};

template <>
struct Custom_Serialization<std::string> {
    static void serialize_struct(Serializer& srl, std::string& v) {
        size_t count = v.size();
        serialize(srl, count);
        if (!srl.is_writer) {
            v = std::string(count, '\0');
        }
        if (count == 0) return;
        if (srl.is_writer) {
            write(srl, v.data(), sizeof(v[0]) * count);
        } else {
            read(srl, v.data(), sizeof(v[0]) * count);
        }
    }
};

#define SERIALIZE_BY_MEMBER(T)                                             \
    template <>                                                            \
    struct Custom_Serialization<T> {                                       \
        static void serialize_struct(Serializer& srl, T& t) {              \
            visit_struct::for_each(t, [&](const char* name, auto& value) { \
                ::serialize_struct(srl, value);                            \
            });                                                            \
        }                                                                  \
    };\
