#include "string.h"

void string_split(
    char* str,
    char delimeter,
    uint32_t max_words,
    char** out_word_ptrs,
    uint32_t* out_word_count
) {
    if (*str == '\0') {
        *out_word_count = 0;
    }

    char* str_ptr = str;
    out_word_ptrs[0] = str_ptr;
    *out_word_count = 1;

    while (*str_ptr != '\0') {
        // Replace delimeter with null terminator
        if (*str_ptr == delimeter) {
            str_ptr[0] = '\0';

            // If the string continues past this delimeter,
            // then the next character is the beginning
            // of a new word
            if (str_ptr[1] != '\0') {
                out_word_ptrs[*out_word_count] = str_ptr + 1;
                *out_word_count = *out_word_count + 1;

                // If we now have max_words, return
                if (*out_word_count == max_words) {
                    return;
                }
            }
        }
        str_ptr++;
    }
}
