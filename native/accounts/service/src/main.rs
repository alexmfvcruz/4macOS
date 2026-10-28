use fourstory_account_service::{router, store::Store, Service};
use std::{net::SocketAddr, path::Path};

#[tokio::main]
async fn main() {
    if run().await.is_err() {
        eprintln!("Account service failed. Check configuration, schema and connectivity; credentials are not logged.");
        std::process::exit(1);
    }
}
async fn run() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 4 {
        eprintln!("Usage: fourstory-account-service <secret-file> <database> migrate\n       fourstory-account-service <secret-file> <database> serve <bind-address> <certificate.pem> <key.pem>\n       fourstory-account-service <secret-file> <database> loopback-http <127.0.0.1:port>\nNo migration runs during serve. loopback-http is local development only.");
        return Err("Missing arguments".into());
    }
    let store = Store::from_file(Path::new(&args[1]), &args[2])?;
    if args[3] == "migrate" && args.len() == 4 {
        store.migrate().await?;
        println!("PASS additive account migration");
        return Ok(());
    }
    let bind: SocketAddr = args.get(4).ok_or("Missing bind")?.parse()?;
    let app = router(Service::new(store));
    match args[3].as_str() {
        "serve" if args.len() == 7 => {
            let _ = rustls::crypto::ring::default_provider().install_default();
            let tls =
                axum_server::tls_rustls::RustlsConfig::from_pem_file(&args[5], &args[6]).await?;
            println!("Account service HTTPS listening on {bind}");
            axum_server::bind_rustls(bind, tls)
                .serve(app.into_make_service_with_connect_info::<SocketAddr>())
                .await?;
        }
        "loopback-http" if args.len() == 5 && bind.ip().is_loopback() => {
            println!("Account service local development HTTP listening on {bind}");
            axum::serve(
                tokio::net::TcpListener::bind(bind).await?,
                app.into_make_service_with_connect_info::<SocketAddr>(),
            )
            .await?;
        }
        _ => return Err("Invalid mode or insecure remote bind".into()),
    }
    Ok(())
}
