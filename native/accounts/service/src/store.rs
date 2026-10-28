use fourstory_account_core::Registration;
use sha1::{Digest, Sha1};
use std::{path::Path, time::Duration};
use tiberius::{AuthMethod, Client, Config};
use tokio::net::TcpStream;
use tokio_util::compat::{Compat, TokioAsyncWriteCompatExt};
use zeroize::Zeroizing;

type Connection = Client<Compat<TcpStream>>;

/// No Debug implementation: configuration includes a SQL password.
pub struct Store {
    config: Config,
}

#[derive(Debug, Eq, PartialEq)]
pub enum CreateResult {
    Created,
    Taken,
}

impl Store {
    /// Read credentials from a private file, never command-line arguments.
    /// The audit server's self-signed certificate is allowed only on loopback.
    pub fn from_file(path: &Path, database: &str) -> Result<Self, &'static str> {
        if !database
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'_')
            || database.is_empty()
        {
            return Err("Invalid SQL database name");
        }
        let content = Zeroizing::new(
            std::fs::read_to_string(path).map_err(|_| "Cannot read SQL secret file")?,
        );
        let secret = content
            .lines()
            .find_map(|l| l.strip_prefix("MSSQL_SA_PASSWORD="))
            .ok_or("Missing SQL secret")?;
        let mut config = Config::new();
        config.host("127.0.0.1");
        config.port(14330);
        config.database(database);
        config.authentication(AuthMethod::sql_server(
            content
                .lines()
                .find_map(|l| l.strip_prefix("MSSQL_USER="))
                .unwrap_or("sa"),
            secret,
        ));
        config.trust_cert(); // fixed loopback-only SQL resource, never HTTPS client
        Ok(Self { config })
    }

    async fn connect(&self) -> Result<Connection, &'static str> {
        tokio::time::timeout(Duration::from_secs(10), async {
            let stream = TcpStream::connect(self.config.get_addr())
                .await
                .map_err(|_| "SQL unavailable")?;
            stream.set_nodelay(true).map_err(|_| "SQL unavailable")?;
            Client::connect(self.config.clone(), stream.compat_write())
                .await
                .map_err(|_| "SQL connection failed")
        })
        .await
        .map_err(|_| "SQL connection timeout")?
    }

    pub async fn migrate(&self) -> Result<(), &'static str> {
        let mut connection = self.connect().await?;
        connection
            .simple_query({
                let procedure = format!("CREATE OR ALTER PROCEDURE dbo.NativeRegisterAccount @P1 varchar(50), @P2 char(40), @P3 varchar(254) WITH EXECUTE AS OWNER AS BEGIN\n{}\nEND", include_str!("../../sql/create-account.sql"));
                include_str!("../../sql/001-registration.sql").replace("-- ACCOUNT_PROCEDURE", &format!("EXEC(N'{}');", procedure.replace("'", "''")))
            })
            .await
            .map_err(|_| "Migration failed; inspect schema and duplicate names privately")?
            .into_results()
            .await
            .map_err(|_| "Migration failed")?;
        Ok(())
    }

    pub async fn create(&self, input: &Registration<'_>) -> Result<CreateResult, &'static str> {
        let mut hash = Sha1::new();
        hash.update(input.password.expose().as_bytes());
        let digest = Zeroizing::new(format!("{:x}", hash.finalize()));
        let mut connection = self.connect().await?;
        let rows = connection
            .query(
                "EXEC dbo.NativeRegisterAccount @P1,@P2,@P3",
                &[
                    &input.account.as_str(),
                    &digest.as_str(),
                    &input.email.map(|e| e.as_str()),
                ],
            )
            .await
            .map_err(|_| "Account transaction failed")?
            .into_first_result()
            .await
            .map_err(|_| "Account transaction failed")?;
        match rows.first().and_then(|r| r.get::<i32, _>("result")) {
            Some(0) => Ok(CreateResult::Created),
            Some(1) => Ok(CreateResult::Taken),
            _ => Err("Account transaction returned no result"),
        }
    }
}
