//! C ABI for the existing game's UI. No registration success or authentication
//! is reported here: these exports validate fields and propose random names only.

use fourstory_account_core::{generate_name, FieldError, Registration, SystemEntropy};
use std::{
    panic::{catch_unwind, AssertUnwindSafe},
    slice, str,
};

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Status {
    Ok = 0,
    AccountName = 1,
    Email = 2,
    Password = 3,
    Confirmation = 4,
    InvalidArgument = 5,
    EntropyUnavailable = 6,
    BufferTooSmall = 7,
    Internal = 8,
}

impl From<FieldError> for Status {
    fn from(value: FieldError) -> Self {
        match value {
            FieldError::AccountName => Self::AccountName,
            FieldError::Email => Self::Email,
            FieldError::Password => Self::Password,
            FieldError::Confirmation => Self::Confirmation,
        }
    }
}

// Caller guarantees readable, non-null storage for every nonzero length. A zero
// length can have a null pointer. Reject lengths before constructing a slice.
unsafe fn text<'a>(
    ptr: *const u8,
    len: usize,
    max: usize,
    field: Status,
) -> Result<&'a str, Status> {
    if len > max {
        return Err(field);
    }
    if len == 0 {
        return Ok("");
    }
    if ptr.is_null() {
        return Err(Status::InvalidArgument);
    }
    str::from_utf8(unsafe { slice::from_raw_parts(ptr, len) }).map_err(|_| field)
}

/// Validate byte buffers without copying, logging or retaining the password.
///
/// # Safety
/// Nonempty input buffers must remain readable for the call. No input may be
/// concurrently modified. Status::Ok means valid fields, NOT account creation.
#[no_mangle]
pub unsafe extern "C" fn fourstory_accounts_validate(
    name: *const u8,
    name_len: usize,
    email: *const u8,
    email_len: usize,
    password: *const u8,
    password_len: usize,
    confirmation: *const u8,
    confirmation_len: usize,
) -> Status {
    catch_unwind(AssertUnwindSafe(|| -> Result<(), Status> {
        let name = unsafe { text(name, name_len, 50, Status::AccountName) }?;
        let email = unsafe { text(email, email_len, 254, Status::Email) }?;
        let password = unsafe { text(password, password_len, 64, Status::Password) }?;
        let confirmation =
            unsafe { text(confirmation, confirmation_len, 64, Status::Confirmation) }?;
        Registration::validate(name, email, password, confirmation).map_err(Status::from)?;
        Ok(())
    }))
    .map_or(Status::Internal, |r| r.err().unwrap_or(Status::Ok))
}

/// Write a proposed name plus NUL; the written length excludes the terminator.
///
/// # Safety
/// `written` must be writable for one usize and must not overlap output. For
/// capacity >=16, output must reference at least capacity writable bytes. On
/// failure, written is zero and output[0] is zero when capacity >=16.
#[no_mangle]
pub unsafe extern "C" fn fourstory_accounts_generate_name(
    output: *mut u8,
    capacity: usize,
    written: *mut usize,
) -> Status {
    if written.is_null() {
        return Status::InvalidArgument;
    }
    unsafe { written.write(0) };
    if capacity < 16 {
        return Status::BufferTooSmall;
    }
    if output.is_null() {
        return Status::InvalidArgument;
    }
    unsafe { output.write(0) };
    catch_unwind(AssertUnwindSafe(|| {
        let mut source = match SystemEntropy::open() {
            Ok(value) => value,
            Err(_) => return Status::EntropyUnavailable,
        };
        let name = match generate_name(&mut source) {
            Ok(value) => value,
            Err(_) => return Status::EntropyUnavailable,
        };
        unsafe {
            std::ptr::copy_nonoverlapping(name.as_ptr(), output, name.len());
            output.add(name.len()).write(0);
            written.write(name.len());
        }
        Status::Ok
    }))
    .unwrap_or(Status::Internal)
}

mod registration;

/// Begin a create-only request, copying validated inputs into a bounded worker.
/// # Safety
/// Same readable-buffer requirements as fourstory_accounts_validate.
#[no_mangle]
pub unsafe extern "C" fn fourstory_accounts_register(
    name: *const u8,
    nl: usize,
    email: *const u8,
    el: usize,
    password: *const u8,
    pl: usize,
    confirmation: *const u8,
    cl: usize,
) -> i32 {
    catch_unwind(AssertUnwindSafe(|| unsafe {
        registration::start([(name, nl), (email, el), (password, pl), (confirmation, cl)])
    }))
    .unwrap_or(Status::Internal as i32)
}
#[no_mangle]
pub extern "C" fn fourstory_accounts_poll() -> i32 {
    registration::poll()
}
#[no_mangle]
pub extern "C" fn fourstory_accounts_cancel() {
    registration::cancel()
}
