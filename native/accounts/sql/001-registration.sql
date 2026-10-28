-- Additive migration. Refuse ambiguous legacy identities; never repair/delete them.
SET XACT_ABORT ON;
SET LOCK_TIMEOUT 5000;
BEGIN TRANSACTION;
DECLARE @lock int;
EXEC @lock=sys.sp_getapplock @Resource=N'fourstory-registration-migration',
 @LockMode='Exclusive',@LockOwner='Transaction',@LockTimeout=5000;
IF @lock<0 THROW 51000,'Registration migration lock unavailable.',1;
IF EXISTS(SELECT szUserID FROM dbo.TACCOUNT_PW WITH(TABLOCKX,HOLDLOCK)
 GROUP BY szUserID HAVING COUNT(*)>1)
 THROW 51001,'Duplicate legacy account names require manual review.',1;
IF NOT EXISTS(SELECT 1 FROM sys.indexes WHERE object_id=OBJECT_ID(N'dbo.TACCOUNT_PW')
 AND name=N'UX_FourStory_AccountName')
 CREATE UNIQUE INDEX UX_FourStory_AccountName ON dbo.TACCOUNT_PW(szUserID);
IF OBJECT_ID(N'dbo.NativeAccountProfile',N'U') IS NULL
 CREATE TABLE dbo.NativeAccountProfile(
  dwUserID int NOT NULL CONSTRAINT PK_NativeAccountProfile PRIMARY KEY,
  Email varchar(254) NULL,
  CreatedAt datetime2 NOT NULL CONSTRAINT DF_NativeAccountProfile_Created DEFAULT SYSUTCDATETIME(),
  CONSTRAINT FK_NativeAccountProfile_Account FOREIGN KEY(dwUserID) REFERENCES dbo.TACCOUNT_PW(dwUserID)
 );
-- ACCOUNT_PROCEDURE
COMMIT;
