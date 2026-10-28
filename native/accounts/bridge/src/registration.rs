//! One bounded background request; the game polls on its own thread. A canceled
//! request may already have committed in SQL, but cannot deliver a stale UI result.
use super::{text, Status};
use fourstory_account_core::Registration;
use reqwest::{blocking::Client, redirect::Policy, Url};
use serde::{Deserialize, Serialize};
use std::{
    sync::{
        atomic::{AtomicBool, Ordering},
        Mutex,
    },
    time::Duration,
};
use zeroize::Zeroizing;

static BUSY: AtomicBool = AtomicBool::new(false);
static JOB: Mutex<(u64, i32)> = Mutex::new((0, 0));
pub const PENDING: i32 = 20;
pub const CREATED: i32 = 21;
pub const TAKEN: i32 = 22;
pub const UNAVAILABLE: i32 = 23;
pub const BUSY_STATUS: i32 = 24;
pub const CONFIG: i32 = 25;

#[derive(Serialize)]
struct Request<'a> {
    account: &'a str,
    email: &'a str,
    password: &'a str,
    confirmation: &'a str,
}
#[derive(Deserialize)]
struct Reply {
    code: String,
}

fn endpoint(value: &str, development: bool) -> Result<Url, ()> {
    let url = Url::parse(value).map_err(|_| ())?;
    if !url.username().is_empty()
        || url.password().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
    {
        return Err(());
    }
    let loopback = url
        .host_str()
        .is_some_and(|host| host == "127.0.0.1" || host == "[::1]");
    if url.scheme() != "https" && !(development && loopback && url.scheme() == "http") {
        return Err(());
    }
    if url.path() != "/v1/accounts" {
        return Err(());
    }
    Ok(url)
}

pub unsafe fn start(buffers: [(*const u8, usize); 4]) -> i32 {
    let [(name, nl), (email, el), (password, pl), (confirmation, cl)] = buffers;
    let result = (|| -> Result<i32, Status> {
        let name = unsafe { text(name, nl, 50, Status::AccountName) }?;
        let email = unsafe { text(email, el, 254, Status::Email) }?;
        let password = unsafe { text(password, pl, 64, Status::Password) }?;
        let confirmation = unsafe { text(confirmation, cl, 64, Status::Confirmation) }?;
        Registration::validate(name, email, password, confirmation).map_err(Status::from)?;
        let Ok(value) = std::env::var("FOURSTORY_ACCOUNTS_URL") else {
            return Ok(CONFIG);
        };
        let Ok(url) = endpoint(
            &value,
            std::env::var("FOURSTORY_ACCOUNTS_DEV_HTTP").as_deref() == Ok("1"),
        ) else {
            return Ok(CONFIG);
        };
        if BUSY
            .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
            .is_err()
        {
            return Ok(BUSY_STATUS);
        }
        let mut job = JOB.lock().map_err(|_| {
            BUSY.store(false, Ordering::Release);
            Status::Internal
        })?;
        job.0 = job.0.checked_add(1).ok_or(Status::Internal)?;
        job.1 = PENDING;
        let generation = job.0;
        let name = name.to_owned();
        let email = email.to_owned();
        let password = Zeroizing::new(password.to_owned());
        let confirmation = Zeroizing::new(confirmation.to_owned());
        let spawn = std::thread::Builder::new()
            .name("account-registration".into())
            .spawn(move || {
                let result = (|| -> Result<i32, ()> {
                    let mut builder = Client::builder()
                        .no_proxy()
                        .redirect(Policy::none())
                        .connect_timeout(Duration::from_secs(3))
                        .timeout(Duration::from_secs(15));
                    if let Ok(path) = std::env::var("FOURSTORY_ACCOUNTS_CA") {
                        let bytes = std::fs::read(path).map_err(|_| ())?;
                        builder = builder.add_root_certificate(
                            reqwest::Certificate::from_pem(&bytes).map_err(|_| ())?,
                        );
                    }
                    let response = builder
                        .build()
                        .map_err(|_| ())?
                        .post(url)
                        .json(&Request {
                            account: &name,
                            email: &email,
                            password: &password,
                            confirmation: &confirmation,
                        })
                        .send()
                        .map_err(|_| ())?;
                    let status = response.status().as_u16();
                    // The server owns the body; bound it even if the configured endpoint misbehaves.
                    let mut bytes = Vec::new();
                    use std::io::Read;
                    response
                        .take(1025)
                        .read_to_end(&mut bytes)
                        .map_err(|_| ())?;
                    if bytes.len() > 1024 {
                        return Err(());
                    }
                    let reply: Reply = serde_json::from_slice(&bytes).map_err(|_| ())?;
                    match (status, reply.code.as_str()) {
                        (201, "created") => Ok(CREATED),
                        (409, "account_taken") => Ok(TAKEN),
                        _ => Err(()),
                    }
                })()
                .unwrap_or(UNAVAILABLE);
                if let Ok(mut job) = JOB.lock() {
                    if job.0 == generation {
                        job.1 = result;
                    }
                }
                BUSY.store(false, Ordering::Release);
            });
        if spawn.is_err() {
            job.1 = UNAVAILABLE;
            BUSY.store(false, Ordering::Release);
            return Ok(UNAVAILABLE);
        }
        Ok(PENDING)
    })();
    result.unwrap_or_else(|s| s as i32)
}

pub fn poll() -> i32 {
    JOB.lock().map_or(Status::Internal as i32, |j| j.1)
}
pub fn cancel() {
    if let Ok(mut j) = JOB.lock() {
        j.0 = j.0.saturating_add(1);
        j.1 = 0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn transport_policy() {
        assert!(endpoint("https://accounts.example/v1/accounts", false).is_ok());
        assert!(endpoint("http://127.0.0.1:1234/v1/accounts", true).is_ok());
        for url in [
            "http://127.0.0.1/v1/accounts",
            "http://remote.example/v1/accounts",
            "https://a:b@example/v1/accounts",
            "https://example/v1/accounts?password=x",
            "https://example/other",
        ] {
            assert!(endpoint(url, false).is_err());
        }
        assert!(endpoint("http://remote.example/v1/accounts", true).is_err());
    }
    #[test]
    fn cancel_discards_status() {
        cancel();
        assert_eq!(poll(), 0);
    }
}
