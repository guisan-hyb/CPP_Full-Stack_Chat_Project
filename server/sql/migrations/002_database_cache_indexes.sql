USE guisan;

ALTER TABLE `user`
    ADD UNIQUE KEY `uk_user_uid` (`uid`),
    ADD UNIQUE KEY `uk_user_name` (`name`),
    ADD UNIQUE KEY `uk_user_email` (`email`);

ALTER TABLE `friend_apply`
    ADD KEY `idx_apply_recipient` (`to_uid`, `status`, `id`);

ALTER TABLE `friend`
    ADD KEY `idx_friend_reverse` (`friend_id`, `self_id`);
