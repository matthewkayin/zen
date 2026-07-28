#pragma once

#include <cstdint>

void string_split(
    char* str,
    char delimeter,
    uint32_t max_words,
    char** out_word_ptrs,
    uint32_t* out_word_count);

// Assumes prefix is null-terminated but str does not need to be
bool string_begins_with(const char* str, const char* prefix);
