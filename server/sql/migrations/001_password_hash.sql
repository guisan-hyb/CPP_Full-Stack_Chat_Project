USE guisan;

ALTER TABLE `user`
    CHANGE COLUMN `pwd` `password_hash` VARCHAR(255) NOT NULL;

DROP PROCEDURE IF EXISTS `reg_user`;

DELIMITER //
CREATE PROCEDURE `reg_user`(
    IN `new_name` VARCHAR(255),
    IN `new_email` VARCHAR(255),
    IN `new_password_hash` VARCHAR(255),
    OUT `result` INT
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SET result = -1;
    END;

    START TRANSACTION;

    IF EXISTS (SELECT 1 FROM `user` WHERE `name` = new_name) THEN
        SET result = 0;
        COMMIT;
    ELSEIF EXISTS (SELECT 1 FROM `user` WHERE `email` = new_email) THEN
        SET result = 0;
        COMMIT;
    ELSE
        UPDATE `user_id` SET `id` = `id` + 1;
        SELECT `id` INTO @new_id FROM `user_id`;
        INSERT INTO `user` (`uid`, `name`, `email`, `password_hash`)
        VALUES (@new_id, new_name, new_email, new_password_hash);
        SET result = @new_id;
        COMMIT;
    END IF;
END//
DELIMITER ;
