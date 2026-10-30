#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Copies at most n-1 bytes without splitting a UTF-8 sequence; always terminates.
void tx_copy(char *dst, const char *src, size_t n);

// Decodes &amp; &lt; &gt; &quot; &apos; &#NNN; &#xHH; in place (to UTF-8) and
// drops zero-width spaces (&#8203;), which the OEBB feeds sprinkle into text.
void tx_html_decode(char *s);

// Trims ASCII whitespace in place.
void tx_trim(char *s);

// Removes a suffix (exact match) if present; returns true if removed.
bool tx_strip_suffix(char *s, const char *suffix);

// Wiener Linien destinations mark interchanges with a separate "U" or "S" word
// ("Stadion U", "Praterstern S U", "Parlament, U Volkstheater"). Copies the
// name without those words to `name` and the letters, in order and without
// repeats, to `marks` ("SU").
void tx_split_transfers(const char *towards, char *name, size_t n, char *marks, size_t mn);

// Lowercases and strips accents (ä->a, ß->ss, ...) for forgiving searches.
void tx_fold(const char *in, char *out, size_t n);

bool tx_is_utf8(const char *s);

// Returns a malloc'd UTF-8 copy of a Latin-1 string (NULL if out of memory).
char *tx_latin1_to_utf8(const char *s);

// Decodes %XX escapes and '+' (form encoding) in place.
void tx_url_decode(char *s);

// Encodes one code point as UTF-8; returns the number of bytes written (0..4).
int tx_utf8_encode(uint32_t cp, char *out);
