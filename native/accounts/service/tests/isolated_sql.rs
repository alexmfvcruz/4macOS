//! Run only against a newly prepared test database, never TGLOBAL_GSP.
use fourstory_account_core::Registration;
use fourstory_account_service::{
    router,
    store::{CreateResult, Store},
    Service,
};
use std::path::Path;

#[tokio::test]
#[ignore = "requires explicit isolated database fixture"]
async fn create_only_sql_and_http() {
    let db = std::env::var("FOURSTORY_ACCOUNTS_TEST_DB").unwrap();
    assert!(db.starts_with("FourStory_Accounts_Test_"));
    let file = std::env::var("FOURSTORY_ACCOUNTS_TEST_SECRET").unwrap();
    let store = Store::from_file(Path::new(&file), &db).unwrap();
    store.migrate().await.unwrap();
    store.migrate().await.unwrap(); // additive idempotence
    let input = Registration::validate(
        "NewPlayer",
        "player@example.com",
        "NeverLogThis42",
        "NeverLogThis42",
    )
    .unwrap();
    assert_eq!(store.create(&input).await.unwrap(), CreateResult::Created);
    let duplicate = Registration::validate(
        "newplayer",
        "changed@example.com",
        "ChangedSecret42",
        "ChangedSecret42",
    )
    .unwrap();
    assert_eq!(store.create(&duplicate).await.unwrap(), CreateResult::Taken);
    let rejected = Registration::validate(
        "RollbackPlayer",
        "rollback@example.test",
        "NeverLogThis42",
        "NeverLogThis42",
    )
    .unwrap();
    assert!(store.create(&rejected).await.is_err());
    let state = Service::new(store);
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let url = format!("http://{}/v1/accounts", listener.local_addr().unwrap());
    let server = tokio::spawn(async move {
        axum::serve(
            listener,
            router(state).into_make_service_with_connect_info::<std::net::SocketAddr>(),
        )
        .await
        .unwrap();
    });
    let client = reqwest::Client::new();
    let body = serde_json::json!({"account":"HttpPlayer","email":"","password":"NeverLogThis42","confirmation":"NeverLogThis42"});
    assert_eq!(
        client
            .post(&url)
            .header("Origin", "https://untrusted.example")
            .json(&body)
            .send()
            .await
            .unwrap()
            .status(),
        403
    );
    assert_eq!(
        client
            .post(&url)
            .header("Content-Type", "application/json")
            .body("not json")
            .send()
            .await
            .unwrap()
            .status(),
        400
    );
    let mut invalid = body.clone();
    invalid["account"] = "bad name".into();
    assert_eq!(
        client
            .post(&url)
            .json(&invalid)
            .send()
            .await
            .unwrap()
            .status(),
        422
    );
    let (a, b) = tokio::join!(
        client.post(&url).json(&body).send(),
        client.post(&url).json(&body).send()
    );
    let mut codes = [a.unwrap().status().as_u16(), b.unwrap().status().as_u16()];
    codes.sort();
    assert_eq!(codes, [201, 409]);
    let response = client
        .post(&url)
        .header("Content-Type", "application/json")
        .body("x".repeat(4096))
        .send()
        .await
        .unwrap();
    assert_eq!(response.status(), 400); // bounded extractor rejection has generic body
    assert!(!response.text().await.unwrap().contains("NeverLog"));
    for _ in 0..10 {
        let _ = client.post(&url).json(&body).send().await.unwrap();
    }
    assert_eq!(
        client.post(&url).json(&body).send().await.unwrap().status(),
        429
    );
    server.abort();
}
