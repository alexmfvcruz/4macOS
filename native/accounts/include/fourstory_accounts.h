#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum FourStoryAccountStatus {
    FOURSTORY_ACCOUNT_OK = 0, /* Field validation only; never registration success. */
    FOURSTORY_ACCOUNT_NAME = 1,
    FOURSTORY_ACCOUNT_EMAIL = 2,
    FOURSTORY_ACCOUNT_PASSWORD = 3,
    FOURSTORY_ACCOUNT_CONFIRMATION = 4,
    FOURSTORY_ACCOUNT_INVALID_ARGUMENT = 5,
    FOURSTORY_ACCOUNT_ENTROPY_UNAVAILABLE = 6,
    FOURSTORY_ACCOUNT_BUFFER_TOO_SMALL = 7,
    FOURSTORY_ACCOUNT_INTERNAL = 8
};

/* Valid buffers must remain readable and unmodified throughout the call.
 * Nonzero lengths require valid pointers; zero lengths may use NULL.
 * No credential is copied, retained, logged or sent by this function. */
int32_t fourstory_accounts_validate(
    const uint8_t* name, size_t name_len,
    const uint8_t* email, size_t email_len,
    const uint8_t* password, size_t password_len,
    const uint8_t* confirmation, size_t confirmation_len);

/* A name is proposed, never reserved. capacity must be >=16 for success.
 * written must be writable and not overlap output; it is zero on failure.
 * output[0] is cleared on failure when capacity>=16 and output is valid. */
int32_t fourstory_accounts_generate_name(uint8_t* output, size_t capacity, size_t* written);

/* Background registration statuses: 20 pending, 21 committed, 22 name taken,
 * 23 unavailable (commit may be unknown), 24 worker busy, 25 URL not configured.
 * Cancel discards the result; it cannot undo an already committed account. */
int32_t fourstory_accounts_register(const uint8_t*,size_t,const uint8_t*,size_t,
                                  const uint8_t*,size_t,const uint8_t*,size_t);
int32_t fourstory_accounts_poll(void);
void fourstory_accounts_cancel(void);

#ifdef __cplusplus
}
#endif
