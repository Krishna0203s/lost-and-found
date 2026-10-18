#include "validation.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

int validate_item_type(const char* type) {
    if (!type) return 0;
    return (strcasecmp(type, "LOST") == 0 || strcasecmp(type, "FOUND") == 0);
}

int validate_priority(const char* priority) {
    if (!priority) return 0;
    return (strcasecmp(priority, "NORMAL") == 0 ||
            strcasecmp(priority, "IMPORTANT") == 0 ||
            strcasecmp(priority, "VERY_IMPORTANT") == 0);
}

int validate_status(const char* status) {
    if (!status) return 0;
    return (strcasecmp(status, "LOST") == 0 ||
            strcasecmp(status, "FOUND") == 0 ||
            strcasecmp(status, "MATCHED") == 0 ||
            strcasecmp(status, "CLAIMED") == 0 ||
            strcasecmp(status, "RETURNED") == 0);
}

int validate_status_transition(const char* current_status, const char* new_status) {
    if (!current_status || !new_status) return 0;
    if (strcasecmp(current_status, new_status) == 0) return 1; /* idempotent */

    /* LOST -> MATCHED, RETURNED */
    if (strcasecmp(current_status, "LOST") == 0) {
        return (strcasecmp(new_status, "MATCHED") == 0 ||
                strcasecmp(new_status, "RETURNED") == 0);
    }

    /* FOUND -> MATCHED, CLAIMED, RETURNED */
    if (strcasecmp(current_status, "FOUND") == 0) {
        return (strcasecmp(new_status, "MATCHED") == 0 ||
                strcasecmp(new_status, "CLAIMED") == 0 ||
                strcasecmp(new_status, "RETURNED") == 0);
    }

    /* MATCHED -> CLAIMED, RETURNED, or back to LOST/FOUND if match unconfirmed */
    if (strcasecmp(current_status, "MATCHED") == 0) {
        return (strcasecmp(new_status, "CLAIMED") == 0 ||
                strcasecmp(new_status, "RETURNED") == 0 ||
                strcasecmp(new_status, "LOST") == 0 ||
                strcasecmp(new_status, "FOUND") == 0);
    }

    /* CLAIMED -> RETURNED, or back to FOUND if claim rejected */
    if (strcasecmp(current_status, "CLAIMED") == 0) {
        return (strcasecmp(new_status, "RETURNED") == 0 ||
                strcasecmp(new_status, "FOUND") == 0 ||
                strcasecmp(new_status, "MATCHED") == 0);
    }

    /* RETURNED is a terminal state; cannot be reopened without administrative override */
    if (strcasecmp(current_status, "RETURNED") == 0) {
        return 0;
    }

    return 0;
}

void sanitize_string(char* str) {
    if (!str) return;
    /* Trim leading spaces */
    char* p = str;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != str) {
        memmove(str, p, strlen(p) + 1);
    }
    /* Trim trailing spaces */
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

int validate_item_fields(const Item* item, char* error_buf, size_t buf_len) {
    if (!item) {
        snprintf(error_buf, buf_len, "Null item reference provided");
        return 0;
    }

    if (strlen(item->item_name) < 2) {
        snprintf(error_buf, buf_len, "Item name must be at least 2 characters");
        return 0;
    }

    if (strlen(item->category) < 2) {
        snprintf(error_buf, buf_len, "Category is required");
        return 0;
    }

    if (strlen(item->location) < 2) {
        snprintf(error_buf, buf_len, "Location is required");
        return 0;
    }

    if (strlen(item->reported_by) < 2) {
        snprintf(error_buf, buf_len, "Reporter name is required");
        return 0;
    }

    if (!validate_item_type(item->item_type)) {
        snprintf(error_buf, buf_len, "Invalid item type: must be LOST or FOUND");
        return 0;
    }

    if (strlen(item->priority) > 0 && !validate_priority(item->priority)) {
        snprintf(error_buf, buf_len, "Invalid priority: must be NORMAL, IMPORTANT, or VERY_IMPORTANT");
        return 0;
    }

    return 1;
}

int validate_claim_fields(const Claim* claim, char* error_buf, size_t buf_len) {
    if (!claim) {
        snprintf(error_buf, buf_len, "Null claim reference provided");
        return 0;
    }

    if (claim->found_item_id <= 0) {
        snprintf(error_buf, buf_len, "Valid found item ID is required");
        return 0;
    }

    if (strlen(claim->claimant_name) < 2) {
        snprintf(error_buf, buf_len, "Claimant name is required");
        return 0;
    }

    if (strlen(claim->claimant_id) < 2) {
        snprintf(error_buf, buf_len, "Claimant student/staff ID is required");
        return 0;
    }

    if (strlen(claim->claimant_contact) < 4) {
        snprintf(error_buf, buf_len, "Claimant contact email/phone is required");
        return 0;
    }

    if (strlen(claim->verification_response) < 10) {
        snprintf(error_buf, buf_len, "Verification details must be at least 10 characters to prove ownership");
        return 0;
    }

    return 1;
}
