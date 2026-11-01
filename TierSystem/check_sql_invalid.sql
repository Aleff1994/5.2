USE tempdb;
SET NOCOUNT ON;
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_0');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_0.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_0.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_0.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_0.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_0.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_0.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_0.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_0.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_0.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_0.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_0.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_0.NeonItemTierCreate 123,4096,0x020000;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51001 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_0') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid version rejected, temporary objects removed';
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_1');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_1.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_1.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_1.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_1.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_1.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_1.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_1.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_1.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_1.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_1.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_1.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_1.NeonItemTierCreate 123,4096,0x010500;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51001 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_1') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid item_grade rejected, temporary objects removed';
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_2');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_2.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_2.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_2.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_2.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_2.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_2.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_2.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_2.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_2.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_2.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_2.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_2.NeonItemTierCreate 123,4096,0x010001;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51001 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_2') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid truncated rejected, temporary objects removed';
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_3');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_3.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_3.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_3.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_3.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_3.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_3.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_3.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_3.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_3.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_3.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_3.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_3.NeonItemTierCreate 123,4096,0x01000101000005;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51002 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_3') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid option_grade rejected, temporary objects removed';
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_4');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_4.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_4.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_4.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_4.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_4.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_4.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_4.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_4.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_4.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_4.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_4.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_4.NeonItemTierCreate 123,4096,0x01000103000001;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51002 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_4') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid family rejected, temporary objects removed';
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_5');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_5.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_5.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_5.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_5.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_5.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_5.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_5.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_5.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_5.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_5.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_5.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_5.NeonItemTierCreate 123,4096,0x0100020100000101000004;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51002 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_5') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid duplicate rejected, temporary objects removed';
GO
BEGIN TRANSACTION;
EXEC('CREATE SCHEMA codex_tier_invalid_6');
GO
-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'codex_tier_invalid_6.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE codex_tier_invalid_6.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'codex_tier_invalid_6.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_6.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'codex_tier_invalid_6.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE codex_tier_invalid_6.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE codex_tier_invalid_6.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN codex_tier_invalid_6.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE codex_tier_invalid_6.NeonItemTierCreate
 @ItemSerial bigint,@ItemID int,@Metadata varbinary(max)
AS
BEGIN
 SET NOCOUNT ON;
 SET XACT_ABORT ON;
 DECLARE @length int=DATALENGTH(@Metadata),@count int,@pos int;
 IF @ItemSerial IS NULL OR @ItemSerial NOT BETWEEN 1 AND 4294967295 OR
    @ItemID IS NULL OR @ItemID NOT BETWEEN 0 AND 8191 OR
    @Metadata IS NULL OR @length<3 OR @length>131
  THROW 51000,'Invalid item tier identity or payload length',1;
 SET @count=CONVERT(int,SUBSTRING(@Metadata,3,1));
 IF CONVERT(int,SUBSTRING(@Metadata,1,1))<>1 OR CONVERT(int,SUBSTRING(@Metadata,2,1))>4 OR
    @count>32 OR @length<>3+4*@count
  THROW 51001,'Invalid item tier header',1;
 DECLARE @seen TABLE(Family int,OptionID int,PRIMARY KEY(Family,OptionID));
 SET @pos=4;
 WHILE @pos<=@length
 BEGIN
  DECLARE @family int=CONVERT(int,SUBSTRING(@Metadata,@pos,1)),
          @id int=CONVERT(int,SUBSTRING(@Metadata,@pos+1,1))+256*CONVERT(int,SUBSTRING(@Metadata,@pos+2,1)),
          @rank int=CONVERT(int,SUBSTRING(@Metadata,@pos+3,1));
  IF @family NOT IN(1,2) OR @rank NOT BETWEEN 1 AND 4 OR EXISTS(SELECT 1 FROM @seen WHERE Family=@family AND OptionID=@id)
   THROW 51002,'Invalid or duplicate option tier',1;
  INSERT @seen VALUES(@family,@id);
  SET @pos+=4;
 END;
 DECLARE @existing varbinary(131),@existingID int,@status int;
 BEGIN TRANSACTION;
 -- Range lock serializes simultaneous creation of the same serial.
 SELECT @existing=Metadata,@existingID=OriginalItemID
 FROM codex_tier_invalid_6.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT codex_tier_invalid_6.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;

DECLARE @caught int=0;
BEGIN TRY
 EXEC codex_tier_invalid_6.NeonItemTierCreate 123,4096,NULL;
END TRY
BEGIN CATCH
 SET @caught=ERROR_NUMBER();
 IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
END CATCH;
IF XACT_STATE()<>0 ROLLBACK TRANSACTION;
IF @caught<>51000 THROW 51020,'Invalid payload was not rejected correctly',1;
IF SCHEMA_ID('codex_tier_invalid_6') IS NOT NULL THROW 51021,'Temporary schema cleanup failed',1;
PRINT 'PASS: invalid null rejected, temporary objects removed';
GO
