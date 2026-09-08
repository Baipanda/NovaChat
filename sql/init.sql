-- NovaChat schema (from 基于muduo网络库的集群服务器项目.pdf)
-- Default app config in C++: root / 100103 @ 127.0.0.1 db=chat
-- Demo docker MySQL: port 3307, password 100103

CREATE DATABASE IF NOT EXISTS chat DEFAULT CHARACTER SET utf8mb4;
USE chat;

CREATE TABLE IF NOT EXISTS user (
  id INT PRIMARY KEY AUTO_INCREMENT,
  name VARCHAR(50) NOT NULL UNIQUE,
  password VARCHAR(50) NOT NULL,
  state ENUM('online', 'offline') NOT NULL DEFAULT 'offline'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS friend (
  userid INT NOT NULL,
  friendid INT NOT NULL,
  PRIMARY KEY (userid, friendid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS allgroup (
  id INT PRIMARY KEY AUTO_INCREMENT,
  groupname VARCHAR(50) NOT NULL UNIQUE,
  groupdesc VARCHAR(200) DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS groupuser (
  groupid INT NOT NULL,
  userid INT NOT NULL,
  grouprole ENUM('creator', 'normal') NOT NULL DEFAULT 'normal',
  PRIMARY KEY (groupid, userid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS offlinemessage (
  userid INT NOT NULL,
  message VARCHAR(500) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- seed
INSERT INTO user (id, name, password, state) VALUES
  (1, 'alice', '123456', 'offline'),
  (2, 'bob', '123456', 'offline'),
  (3, 'carol', '123456', 'offline')
ON DUPLICATE KEY UPDATE name=VALUES(name);

INSERT IGNORE INTO friend (userid, friendid) VALUES
  (1, 2), (2, 1),
  (1, 3), (3, 1),
  (2, 3), (3, 2);

INSERT INTO allgroup (id, groupname, groupdesc) VALUES
  (1, 'NovaChat', 'demo group')
ON DUPLICATE KEY UPDATE groupname=VALUES(groupname);

INSERT IGNORE INTO groupuser (groupid, userid, grouprole) VALUES
  (1, 1, 'creator'),
  (1, 2, 'normal'),
  (1, 3, 'normal');
