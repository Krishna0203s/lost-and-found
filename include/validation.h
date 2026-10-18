#ifndef VALIDATION_H
#define VALIDATION_H

#include "models.h"

/* Validates item type ("LOST" or "FOUND"). Returns 1 if valid, 0 otherwise. */
int validate_item_type(const char* type);

/* Validates priority level ("NORMAL", "IMPORTANT", "VERY_IMPORTANT"). Returns 1 if valid, 0 otherwise. */
int validate_priority(const char* priority);

/* Validates status string. Returns 1 if valid, 0 otherwise. */
int validate_status(const char* status);

/* Validates status state transition rules. Returns 1 if permitted, 0 otherwise. */
int validate_status_transition(const char* current_status, const char* new_status);

/* Validates complete item fields. Writes error to error_buf if invalid. Returns 1 if valid, 0 if error. */
int validate_item_fields(const Item* item, char* error_buf, size_t buf_len);

/* Validates claim submission fields. Returns 1 if valid, 0 if error. */
int validate_claim_fields(const Claim* claim, char* error_buf, size_t buf_len);

/* Trims whitespace and strips dangerous characters. */
void sanitize_string(char* str);

#endif /* VALIDATION_H */
