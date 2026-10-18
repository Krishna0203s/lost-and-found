#include "matching.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

static char* portable_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    size_t nlen = strlen(needle);
    if (nlen == 0) return (char*)haystack;
    for (; *haystack; haystack++) {
#ifdef _WIN32
        if (_strnicmp(haystack, needle, nlen) == 0) return (char*)haystack;
#else
        if (strncasecmp(haystack, needle, nlen) == 0) return (char*)haystack;
#endif
    }
    return NULL;
}
#define strcasestr portable_strcasestr

#define MAX_TOKENS 64
#define MAX_WORD_LEN 32

/* Common stop words to exclude when comparing descriptions */
static const char* STOP_WORDS[] = {
    "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for", "of",
    "with", "by", "from", "up", "about", "into", "through", "after", "is", "was",
    "are", "were", "it", "its", "my", "me", "this", "that", "has", "have", "had",
    "item", "lost", "found", NULL
};

static int is_stop_word(const char* word) {
    for (int i = 0; STOP_WORDS[i] != NULL; i++) {
        if (strcasecmp(word, STOP_WORDS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Extract normalized tokens from text */
static int tokenize_text(const char* text, char tokens[MAX_TOKENS][MAX_WORD_LEN]) {
    if (!text) return 0;
    int count = 0;
    const char* p = text;

    while (*p && count < MAX_TOKENS) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        if (!*p) break;

        char buf[MAX_WORD_LEN];
        int len = 0;
        while (*p && isalnum((unsigned char)*p) && len < MAX_WORD_LEN - 1) {
            buf[len++] = tolower((unsigned char)*p);
            p++;
        }
        buf[len] = '\0';

        if (len >= 3 && !is_stop_word(buf)) {
            /* Check if duplicate */
            int dup = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(tokens[i], buf) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) {
                snprintf(tokens[count], MAX_WORD_LEN, "%s", buf);
                count++;
            }
        }
    }
    return count;
}

int score_category(const char* cat1, const char* cat2) {
    if (!cat1 || !cat2) return 0;
    if (strcasecmp(cat1, cat2) == 0) {
        return 40;
    }
    /* Substring / partial match */
    if (strcasestr(cat1, cat2) || strcasestr(cat2, cat1)) {
        return 30;
    }
    return 0;
}

int score_location(const char* loc1, const char* loc2) {
    if (!loc1 || !loc2) return 0;
    if (strcasecmp(loc1, loc2) == 0) {
        return 30;
    }
    /* Partial location match e.g. "Library 2nd Floor" vs "Library" */
    if (strcasestr(loc1, loc2) || strcasestr(loc2, loc1)) {
        return 20;
    }

    /* Common campus building keywords */
    char t1[MAX_TOKENS][MAX_WORD_LEN];
    char t2[MAX_TOKENS][MAX_WORD_LEN];
    int c1 = tokenize_text(loc1, t1);
    int c2 = tokenize_text(loc2, t2);

    for (int i = 0; i < c1; i++) {
        for (int j = 0; j < c2; j++) {
            if (strcmp(t1[i], t2[j]) == 0) {
                return 20;
            }
        }
    }
    return 0;
}

int score_description(const char* desc1, const char* desc2) {
    if (!desc1 || !desc2) return 0;
    if (strcasecmp(desc1, desc2) == 0) {
        return 20;
    }

    char t1[MAX_TOKENS][MAX_WORD_LEN];
    char t2[MAX_TOKENS][MAX_WORD_LEN];
    int c1 = tokenize_text(desc1, t1);
    int c2 = tokenize_text(desc2, t2);

    if (c1 == 0 || c2 == 0) return 0;

    int matches = 0;
    for (int i = 0; i < c1; i++) {
        for (int j = 0; j < c2; j++) {
            if (strcmp(t1[i], t2[j]) == 0 ||
                (strlen(t1[i]) > 4 && strlen(t2[j]) > 4 &&
                 (strstr(t1[i], t2[j]) || strstr(t2[j], t1[i])))) {
                matches++;
                break;
            }
        }
    }

    if (matches >= 3) return 20;
    if (matches == 2) return 15;
    if (matches == 1) return 10;
    return 0;
}

int score_date(const char* date1, const char* date2) {
    if (!date1 || !date2) return 0;
    if (strcmp(date1, date2) == 0) {
        return 10;
    }

    /* Parse YYYY-MM-DD */
    int y1, m1, d1;
    int y2, m2, d2;
    if (sscanf(date1, "%d-%d-%d", &y1, &m1, &d1) == 3 &&
        sscanf(date2, "%d-%d-%d", &y2, &m2, &d2) == 3) {
        struct tm tm1 = {0};
        struct tm tm2 = {0};
        tm1.tm_year = y1 - 1900;
        tm1.tm_mon = m1 - 1;
        tm1.tm_mday = d1;
        tm2.tm_year = y2 - 1900;
        tm2.tm_mon = m2 - 1;
        tm2.tm_mday = d2;
        time_t t1 = mktime(&tm1);
        time_t t2 = mktime(&tm2);
        if (t1 != (time_t)-1 && t2 != (time_t)-1) {
            double diff = fabs(difftime(t1, t2)) / 86400.0;
            if (diff <= 2.0) return 5;
            if (diff <= 5.0) return 3;
        }
    }
    return 0;
}

int calculate_match_score(const Item* lost_item, const Item* found_item, MatchScore* out_score) {
    if (!lost_item || !found_item || !out_score) return 0;

    memset(out_score, 0, sizeof(MatchScore));
    out_score->lost_item_id = lost_item->item_id;
    out_score->found_item_id = found_item->item_id;

    out_score->category_score = score_category(lost_item->category, found_item->category);
    out_score->location_score = score_location(lost_item->location, found_item->location);

    /* Also include item names into description comparison for better match recall */
    char full_desc_lost[MAX_DESC_LEN + MAX_NAME_LEN];
    char full_desc_found[MAX_DESC_LEN + MAX_NAME_LEN];
    snprintf(full_desc_lost, sizeof(full_desc_lost), "%s %s", lost_item->item_name, lost_item->description);
    snprintf(full_desc_found, sizeof(full_desc_found), "%s %s", found_item->item_name, found_item->description);

    out_score->description_score = score_description(full_desc_lost, full_desc_found);
    out_score->date_score = score_date(lost_item->report_date, found_item->report_date);

    out_score->total_score = out_score->category_score +
                             out_score->location_score +
                             out_score->description_score +
                             out_score->date_score;

    if (out_score->total_score > 100) {
        out_score->total_score = 100;
    }

    out_score->is_possible_match = (out_score->total_score >= 70) ? 1 : 0;
    strncpy(out_score->match_status, out_score->is_possible_match ? "POTENTIAL" : "DISMISSED", sizeof(out_score->match_status) - 1);

    return out_score->is_possible_match;
}
