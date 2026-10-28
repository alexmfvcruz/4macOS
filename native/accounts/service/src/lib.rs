//! Native account registration: create-only SQL transaction, no password reset.
pub mod store;

use axum::{
    body::{to_bytes, Body},
    extract::{rejection::JsonRejection, ConnectInfo, DefaultBodyLimit, Request, State},
    http::StatusCode,
    middleware::{self, Next},
    response::{IntoResponse, Response},
    routing::{get, post},
    Json, Router,
};
use fourstory_account_core::Registration;
use serde::{Deserialize, Serialize};
use std::{
    collections::HashMap,
    net::{IpAddr, SocketAddr},
    sync::{Arc, Mutex},
    time::{Duration, Instant},
};
use store::{CreateResult, Store};
use tokio::sync::Semaphore;
use zeroize::Zeroize;

#[derive(Deserialize, Zeroize)]
#[zeroize(drop)]
#[serde(deny_unknown_fields)]
pub struct Input {
    pub account: String,
    pub email: String,
    pub password: String,
    pub confirmation: String,
}

#[derive(Serialize, Deserialize)]
pub struct Reply {
    pub code: String,
}

pub struct Service {
    pub store: Store,
    permits: Semaphore,
    rate: Mutex<HashMap<IpAddr, (Instant, u32)>>,
}
impl Service {
    pub fn new(store: Store) -> Arc<Self> {
        Arc::new(Self {
            store,
            permits: Semaphore::new(4),
            rate: Mutex::new(HashMap::new()),
        })
    }
    fn allowed(&self, address: IpAddr) -> bool {
        let Ok(mut rates) = self.rate.lock() else {
            return false;
        };
        rates.retain(|_, (time, _)| time.elapsed() < Duration::from_secs(60));
        if rates.len() >= 1024 && !rates.contains_key(&address) {
            return false;
        }
        let rate = rates.entry(address).or_insert((Instant::now(), 0));
        if rate.1 >= 10 {
            return false;
        }
        rate.1 += 1;
        true
    }
}
fn reply(status: StatusCode, code: &str) -> (StatusCode, Json<Reply>) {
    (status, Json(Reply { code: code.into() }))
}

async fn guard(State(state): State<Arc<Service>>, request: Request, next: Next) -> Response {
    if request.headers().contains_key("origin") {
        return reply(StatusCode::FORBIDDEN, "native_client_required").into_response();
    }
    let Some(ConnectInfo(peer)) = request
        .extensions()
        .get::<ConnectInfo<SocketAddr>>()
        .copied()
    else {
        return reply(StatusCode::SERVICE_UNAVAILABLE, "peer_unavailable").into_response();
    };
    if !state.allowed(peer.ip()) {
        return reply(StatusCode::TOO_MANY_REQUESTS, "try_later").into_response();
    }
    let Ok(_permit) = state.permits.try_acquire() else {
        return reply(StatusCode::SERVICE_UNAVAILABLE, "try_later").into_response();
    };
    let (parts, body) = request.into_parts();
    let bytes = match tokio::time::timeout(Duration::from_secs(5), to_bytes(body, 2048)).await {
        Ok(Ok(bytes)) => bytes,
        _ => return reply(StatusCode::BAD_REQUEST, "invalid_request").into_response(),
    };
    next.run(Request::from_parts(parts, Body::from(bytes)))
        .await
}

async fn register(
    State(state): State<Arc<Service>>,
    input: Result<Json<Input>, JsonRejection>,
) -> (StatusCode, Json<Reply>) {
    let Ok(Json(input)) = input else {
        return reply(StatusCode::BAD_REQUEST, "invalid_request");
    };
    let validated = match Registration::validate(
        &input.account,
        &input.email,
        &input.password,
        &input.confirmation,
    ) {
        Ok(value) => value,
        Err(error) => {
            return reply(
                StatusCode::UNPROCESSABLE_ENTITY,
                match error {
                    fourstory_account_core::FieldError::AccountName => "invalid_account",
                    fourstory_account_core::FieldError::Email => "invalid_email",
                    fourstory_account_core::FieldError::Password => "invalid_password",
                    fourstory_account_core::FieldError::Confirmation => "password_mismatch",
                },
            )
        }
    };
    // No outer timeout around a SQL write: dropping an in-flight operation can
    // hide a committed result. SQL failures are never retried automatically.
    match state.store.create(&validated).await {
        Ok(CreateResult::Created) => reply(StatusCode::CREATED, "created"),
        Ok(CreateResult::Taken) => reply(StatusCode::CONFLICT, "account_taken"),
        Err(_) => reply(StatusCode::SERVICE_UNAVAILABLE, "registration_unavailable"),
    }
}

pub fn router(state: Arc<Service>) -> Router {
    Router::new()
        .route("/health", get(|| async { "fourstory-accounts\n" }))
        .route(
            "/v1/accounts",
            post(register).route_layer(middleware::from_fn_with_state(state.clone(), guard)),
        )
        .layer(DefaultBodyLimit::max(2048))
        .with_state(state)
}
