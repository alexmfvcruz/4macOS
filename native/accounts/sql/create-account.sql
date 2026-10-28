-- Parameters only; this batch never updates an existing row. Table locks also
-- exclude legacy writers while allocating above both identity namespaces.
SET XACT_ABORT ON;
SET LOCK_TIMEOUT 5000;
BEGIN TRY
 BEGIN TRANSACTION;
 DECLARE @maximum bigint,@info bigint,@id bigint;
 SELECT @maximum=ISNULL(MAX(CONVERT(bigint,dwUserID)),0)
 FROM dbo.TACCOUNT_PW WITH(TABLOCKX,HOLDLOCK);
 SELECT @info=ISNULL(MAX(CONVERT(bigint,dwUserID)),0)
 FROM dbo.TUSERINFOTABLE WITH(TABLOCKX,HOLDLOCK);
 IF EXISTS(SELECT 1 FROM dbo.TACCOUNT_PW WHERE szUserID=@P1)
 BEGIN
  ROLLBACK;
  SELECT CAST(1 AS int) AS result,CAST(0 AS int) AS user_id;
  RETURN;
 END;
 SET @id=CASE WHEN @info>@maximum THEN @info ELSE @maximum END;
 IF CONVERT(bigint,ISNULL(IDENT_CURRENT('dbo.TACCOUNT_PW'),0))>@id
  SET @id=CONVERT(bigint,IDENT_CURRENT('dbo.TACCOUNT_PW'));
 SET @id=@id+1;
 IF @id>2147483647 THROW 51002,'Account identity exhausted.',1;
 SET IDENTITY_INSERT dbo.TACCOUNT_PW ON;
 INSERT dbo.TACCOUNT_PW(dwUserID,szUserID,szPasswd,bCheck,dFirstLogin,dLastLogin)
 VALUES(CONVERT(int,@id),@P1,@P2,0,GETDATE(),GETDATE());
 SET IDENTITY_INSERT dbo.TACCOUNT_PW OFF;
 INSERT dbo.TUSERINFOTABLE(dwUserID,bCanCreateCharCount,bAgreement)
 VALUES(CONVERT(int,@id),6,1);
 INSERT dbo.NativeAccountProfile(dwUserID,Email) VALUES(CONVERT(int,@id),@P3);
 COMMIT;
 SELECT CAST(0 AS int) AS result,CONVERT(int,@id) AS user_id;
END TRY
BEGIN CATCH
 IF @@TRANCOUNT>0 ROLLBACK;
 SET IDENTITY_INSERT dbo.TACCOUNT_PW OFF;
 THROW;
END CATCH;
