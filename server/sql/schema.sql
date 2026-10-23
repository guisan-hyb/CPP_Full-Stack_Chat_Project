CREATE DATABASE IF NOT EXISTS guisan
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_0900_ai_ci;

USE guisan;

CREATE TABLE IF NOT EXISTS `user_id` (
    `id` INT NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `user_id` (`id`) VALUES (0);

CREATE TABLE IF NOT EXISTS `user` (
    `id` INT NOT NULL AUTO_INCREMENT,
    `uid` INT NOT NULL,
    `name` VARCHAR(255) NOT NULL DEFAULT '',
    `email` VARCHAR(255) NOT NULL DEFAULT '',
    `password_hash` VARCHAR(255) NOT NULL,
    `nick` VARCHAR(255) NOT NULL DEFAULT '',
    `desc` VARCHAR(255) NOT NULL DEFAULT '',
    `sex` INT NOT NULL DEFAULT 0,
    `icon` VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_uid` (`uid`),
    UNIQUE KEY `uk_user_name` (`name`),
    UNIQUE KEY `uk_user_email` (`email`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `friend_apply` (
    `id` BIGINT NOT NULL AUTO_INCREMENT,
    `from_uid` INT NOT NULL,
    `to_uid` INT NOT NULL,
    `status` SMALLINT NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`),
    UNIQUE KEY `from_to` (`from_uid`, `to_uid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `friend` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `self_id` INT NOT NULL,
    `friend_id` INT NOT NULL,
    `back` VARCHAR(255) DEFAULT '',
    PRIMARY KEY (`id`),
    UNIQUE KEY `self_friend` (`self_id`, `friend_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
