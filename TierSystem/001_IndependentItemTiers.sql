-- Run against the intended MU database only when matching binaries are ready.
-- This migration creates a side table. It never rewrites existing inventory blobs.
SET XACT_ABORT ON;
BEGIN TRANSACTION;
IF OBJECT_ID(N'dbo.NeonItemTiers',N'U') IS NULL
BEGIN
 CREATE TABLE dbo.NeonItemTiers (
  ItemSerial bigint NOT NULL PRIMARY KEY,
  OriginalItemID int NOT NULL,
  Metadata varbinary(131) NOT NULL,
  CreatedUtc datetime2(0) NOT NULL DEFAULT SYSUTCDATETIME(),
  CHECK(ItemSerial BETWEEN 1 AND 4294967295),
  CHECK(OriginalItemID BETWEEN 0 AND 8191),
  CHECK(DATALENGTH(Metadata) BETWEEN 3 AND 131)
 );
END;
IF OBJECT_ID(N'dbo.NeonItemTierRead',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE dbo.NeonItemTierRead AS RETURN;');
IF OBJECT_ID(N'dbo.NeonItemTierCreate',N'P') IS NULL
 EXEC(N'CREATE PROCEDURE dbo.NeonItemTierCreate AS RETURN;');
GO
ALTER PROCEDURE dbo.NeonItemTierRead @ItemSerial bigint
AS
BEGIN
 SET NOCOUNT ON;
 SELECT CASE WHEN t.ItemSerial IS NULL THEN 0 ELSE 1 END AS TierStatus,
        t.OriginalItemID AS ItemID,t.Metadata,DATALENGTH(t.Metadata) AS PayloadSize
 FROM (SELECT @ItemSerial AS ItemSerial) AS requested
 LEFT JOIN dbo.NeonItemTiers AS t ON t.ItemSerial=requested.ItemSerial;
END;
GO
ALTER PROCEDURE dbo.NeonItemTierCreate
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
 FROM dbo.NeonItemTiers WITH(UPDLOCK,HOLDLOCK) WHERE ItemSerial=@ItemSerial;
 IF @existing IS NULL
 BEGIN
  INSERT dbo.NeonItemTiers(ItemSerial,OriginalItemID,Metadata) VALUES(@ItemSerial,@ItemID,@Metadata);
  SET @status=1; -- newly created
 END
 ELSE IF @existing=@Metadata AND @existingID=@ItemID SET @status=2; -- idempotent retry
 ELSE SET @status=3; -- conflict: do not overwrite or reroll
 COMMIT TRANSACTION;
 SELECT @status AS TierStatus;
END;
GO
COMMIT TRANSACTION;
