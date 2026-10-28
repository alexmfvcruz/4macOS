//! Account contracts shared by the native UI adapter and registration service.
//!
//! This crate does not create accounts, store passwords, or authenticate users.
//! Registration rules are a conservative new policy within the legacy byte ABI.

use std::fmt;

pub const NAME_MAX: usize = 50;
pub const PASSWORD_MIN: usize = 8;
pub const PASSWORD_MAX: usize = 64;
pub const EMAIL_MAX: usize = 254;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FieldError {
    AccountName,
    Email,
    Password,
    Confirmation,
}

impl FieldError {
    /// Static messages never contain user input or a credential.
    pub fn message(self) -> &'static str {
        match self {
            Self::AccountName => "Use 1–50 ASCII letters, numbers or underscores.",
            Self::Email => "Enter a valid ASCII e-mail address, up to 254 bytes.",
            Self::Password => "Use 8–64 printable ASCII characters for the password.",
            Self::Confirmation => "The passwords do not match.",
        }
    }
}

impl fmt::Display for FieldError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.message())
    }
}

impl std::error::Error for FieldError {}

#[derive(Clone, Copy, Eq, PartialEq)]
pub struct AccountName<'a>(&'a str);

impl<'a> AccountName<'a> {
    pub fn parse(value: &'a str) -> Result<Self, FieldError> {
        if value.is_empty()
            || value.len() > NAME_MAX
            || !value
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || b == b'_')
        {
            return Err(FieldError::AccountName);
        }
        Ok(Self(value))
    }

    pub fn as_str(self) -> &'a str {
        self.0
    }
}

/// Borrowed: validation does not clone a credential or implement Debug/Display.
/// The caller owns the input buffer and must clear it when the operation ends.
#[derive(Clone, Copy)]
pub struct Password<'a>(&'a str);

impl<'a> Password<'a> {
    pub fn parse(value: &'a str) -> Result<Self, FieldError> {
        if !(PASSWORD_MIN..=PASSWORD_MAX).contains(&value.len())
            || !value.bytes().all(|b| (b' '..=b'~').contains(&b))
        {
            return Err(FieldError::Password);
        }
        Ok(Self(value))
    }

    /// Explicit exposure for the legacy hash/transport boundary only.
    pub fn expose(self) -> &'a str {
        self.0
    }
}

#[derive(Clone, Copy, Eq, PartialEq)]
pub struct Email<'a>(&'a str);

impl<'a> Email<'a> {
    /// None is valid: the original game account tables have no e-mail field.
    /// New metadata uses an unquoted ASCII dot-atom address. No ownership or
    /// delivery claim follows from syntax validation.
    pub fn parse_optional(value: &'a str) -> Result<Option<Self>, FieldError> {
        if value.is_empty() {
            return Ok(None);
        }
        if value.len() > EMAIL_MAX || !value.is_ascii() {
            return Err(FieldError::Email);
        }
        let (local, domain) = value.split_once('@').ok_or(FieldError::Email)?;
        if local.is_empty()
            || local.len() > 64
            || local.starts_with('.')
            || local.ends_with('.')
            || local.contains("..")
            || !local
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || b".!#$%&'*+-/=?^_`{|}~".contains(&b))
            || domain.is_empty()
            || !domain.split('.').all(|label| {
                !label.is_empty()
                    && label.len() <= 63
                    && !label.starts_with('-')
                    && !label.ends_with('-')
                    && label
                        .bytes()
                        .all(|b| b.is_ascii_alphanumeric() || b == b'-')
            })
        {
            return Err(FieldError::Email);
        }
        Ok(Some(Self(value)))
    }

    pub fn as_str(self) -> &'a str {
        self.0
    }
}

/// Fully validated input, not a server commit or evidence of account ownership.
/// Deliberately no Debug, Display or serialization of the password.
pub struct Registration<'a> {
    pub account: AccountName<'a>,
    pub email: Option<Email<'a>>,
    pub password: Password<'a>,
}

impl<'a> Registration<'a> {
    pub fn validate(
        account: &'a str,
        email: &'a str,
        password: &'a str,
        confirmation: &str,
    ) -> Result<Self, FieldError> {
        let account = AccountName::parse(account)?;
        let email = Email::parse_optional(email)?;
        let password = Password::parse(password)?;
        if password.expose() != confirmation {
            return Err(FieldError::Confirmation);
        }
        Ok(Self {
            account,
            email,
            password,
        })
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct EntropyUnavailable;

pub trait Entropy {
    /// Must fill the entire buffer using an OS cryptographic randomness source.
    fn fill(&mut self, bytes: &mut [u8]) -> Result<(), EntropyUnavailable>;
}

/// Unix OS randomness. No weak PRNG or predictable fallback is used.
#[cfg(unix)]
pub struct SystemEntropy(std::fs::File);

#[cfg(unix)]
impl SystemEntropy {
    pub fn open() -> Result<Self, EntropyUnavailable> {
        std::fs::File::open("/dev/urandom")
            .map(Self)
            .map_err(|_| EntropyUnavailable)
    }
}

#[cfg(unix)]
impl Entropy for SystemEntropy {
    fn fill(&mut self, bytes: &mut [u8]) -> Result<(), EntropyUnavailable> {
        std::io::Read::read_exact(&mut self.0, bytes).map_err(|_| EntropyUnavailable)
    }
}

/// A proposed account name; availability must be reserved atomically by SQL.
/// Rejection sampling avoids modulo bias and caps retries if entropy fails.
pub fn generate_name(source: &mut impl Entropy) -> Result<String, EntropyUnavailable> {
    const PREFIX: [&str; 8] = [
        "amber", "cedar", "dawn", "echo", "ember", "mist", "nova", "river",
    ];
    const ENDING: [&str; 8] = [
        "bear", "fern", "hawk", "lynx", "moon", "pine", "vale", "wolf",
    ];
    const RANGE: u64 = 1_000_000;
    const LIMIT: u64 = (u32::MAX as u64 + 1) / RANGE * RANGE;
    let mut word_bytes = [0; 2];
    source.fill(&mut word_bytes)?;
    for _ in 0..32 {
        let mut bytes = [0; 4];
        source.fill(&mut bytes)?;
        let value = u32::from_le_bytes(bytes) as u64;
        if value < LIMIT {
            return Ok(format!(
                "{}{}{:06}",
                PREFIX[(word_bytes[0] & 7) as usize],
                ENDING[(word_bytes[1] & 7) as usize],
                value % RANGE
            ));
        }
    }
    Err(EntropyUnavailable)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn account_byte_boundaries_and_legacy_safe_subset() {
        assert!(AccountName::parse("a").is_ok());
        assert!(AccountName::parse(&"A".repeat(50)).is_ok());
        for value in [
            "".to_owned(),
            "A".repeat(51),
            "a b".into(),
            "café".into(),
            "a\0b".into(),
        ] {
            assert!(AccountName::parse(&value).is_err());
        }
        assert_eq!(AccountName::parse("User_12").unwrap().as_str(), "User_12");
    }

    #[test]
    fn passwords_preserve_bytes_and_never_trim() {
        for size in 8..=64 {
            let value = "x".repeat(size);
            assert_eq!(Password::parse(&value).unwrap().expose(), value);
        }
        for value in [
            "x".repeat(7),
            "x".repeat(65),
            "passéword".into(),
            "abc\0defgh".into(),
            "abc\ndefgh".into(),
        ] {
            assert!(Password::parse(&value).is_err());
        }
        assert_eq!(
            Password::parse("  abcdef  ").unwrap().expose(),
            "  abcdef  "
        );
    }

    #[test]
    fn email_dot_atom_and_dns_label_boundaries() {
        for value in ["", "a@example.com", "A+tag@sub.example", "a@localhost"] {
            assert!(Email::parse_optional(value).is_ok());
        }
        let at_limit = format!(
            "{}@{}.{}.{}",
            "a".repeat(64),
            "b".repeat(63),
            "c".repeat(63),
            "d".repeat(61)
        );
        assert_eq!(at_limit.len(), 254);
        assert!(Email::parse_optional(&at_limit).is_ok());
        assert!(Email::parse_optional(&(at_limit + "x")).is_err());
        for value in [
            "@a", "a@", ".a@b", "a.@b", "a..b@c", "a@@b", "a@b..c", "a@-b", "a@b-", "a b@c",
            "a@café",
        ] {
            assert!(
                Email::parse_optional(value).is_err(),
                "invalid syntax accepted"
            );
        }
        assert!(Email::parse_optional(&format!("{}@b", "a".repeat(65))).is_err());
        assert!(Email::parse_optional(&format!("a@{}", "b".repeat(64))).is_err());
    }

    #[test]
    fn registration_confirmation_and_error_messages() {
        let input = Registration::validate("Player", "", "  abcdef  ", "  abcdef  ").unwrap();
        assert_eq!(input.password.expose(), "  abcdef  ");
        assert!(matches!(
            Registration::validate("Player", "", "  abcdef  ", "abcdef"),
            Err(FieldError::Confirmation)
        ));
        let secret = "DoNotEchoSecret42";
        let error = Registration::validate("invalid name", "", secret, secret)
            .err()
            .unwrap();
        assert!(!error.to_string().contains(secret));
    }

    struct Fixed(u8);
    impl Entropy for Fixed {
        fn fill(&mut self, bytes: &mut [u8]) -> Result<(), EntropyUnavailable> {
            bytes.fill(self.0);
            Ok(())
        }
    }
    struct Failing;
    impl Entropy for Failing {
        fn fill(&mut self, _: &mut [u8]) -> Result<(), EntropyUnavailable> {
            Err(EntropyUnavailable)
        }
    }

    #[test]
    fn generator_is_bounded_and_fails_closed() {
        assert_eq!(generate_name(&mut Fixed(0)).unwrap(), "amberbear000000");
        for byte in 0..=254 {
            let name = generate_name(&mut Fixed(byte)).unwrap();
            assert!(name.len() <= 15 && AccountName::parse(&name).is_ok());
        }
        assert!(generate_name(&mut Fixed(255)).is_err());
        assert!(generate_name(&mut Failing).is_err());
    }

    #[cfg(unix)]
    #[test]
    fn actual_os_entropy_produces_valid_names() {
        let mut source = SystemEntropy::open().unwrap();
        for _ in 0..100 {
            assert!(AccountName::parse(&generate_name(&mut source).unwrap()).is_ok());
        }
    }
}
