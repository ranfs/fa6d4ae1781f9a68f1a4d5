-- phpMyAdmin SQL Dump
-- version 2.11.11.3
-- http://www.phpmyadmin.net
--
-- Servidor: localhost
-- Tempo de Geração: Jan 24, 2013 as 12:31 AM
-- Versão do Servidor: 5.0.95
-- Versão do PHP: 5.1.6

SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Banco de Dados: `logrologs`
--

-- --------------------------------------------------------

--
-- Estrutura da tabela `atcommandlog`
--

CREATE TABLE IF NOT EXISTS `atcommandlog` (
  `atcommand_id` mediumint(9) unsigned NOT NULL auto_increment,
  `atcommand_date` datetime NOT NULL default '0000-00-00 00:00:00',
  `account_id` int(11) unsigned NOT NULL default '0',
  `char_id` int(11) unsigned NOT NULL default '0',
  `char_name` varchar(25) NOT NULL default '',
  `map` varchar(11) NOT NULL default '',
  `command` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`atcommand_id`),
  KEY `account_id` (`account_id`),
  KEY `char_id` (`char_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `atcommandlog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `branchlog`
--

CREATE TABLE IF NOT EXISTS `branchlog` (
  `branch_id` mediumint(9) unsigned NOT NULL auto_increment,
  `branch_date` datetime NOT NULL default '0000-00-00 00:00:00',
  `account_id` int(11) NOT NULL default '0',
  `char_id` int(11) NOT NULL default '0',
  `char_name` varchar(25) NOT NULL default '',
  `map` varchar(11) NOT NULL default '',
  PRIMARY KEY  (`branch_id`),
  KEY `account_id` (`account_id`),
  KEY `char_id` (`char_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `branchlog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `chatlog`
--

CREATE TABLE IF NOT EXISTS `chatlog` (
  `id` bigint(20) NOT NULL auto_increment,
  `time` datetime NOT NULL default '0000-00-00 00:00:00',
  `type` enum('O','W','P','G','M') NOT NULL default 'O',
  `type_id` int(11) NOT NULL default '0',
  `src_charid` int(11) NOT NULL default '0',
  `src_accountid` int(11) NOT NULL default '0',
  `src_map` varchar(11) NOT NULL default '',
  `src_map_x` smallint(4) NOT NULL default '0',
  `src_map_y` smallint(4) NOT NULL default '0',
  `dst_charname` varchar(25) NOT NULL default '',
  `message` varchar(150) NOT NULL default '',
  PRIMARY KEY  (`id`),
  KEY `src_accountid` (`src_accountid`),
  KEY `src_charid` (`src_charid`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `chatlog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `harmony_log`
--

CREATE TABLE IF NOT EXISTS `harmony_log` (
  `log_id` int(11) NOT NULL auto_increment,
  `account_id` int(11) NOT NULL,
  `char_name` varchar(24) NOT NULL,
  `date` timestamp NOT NULL default CURRENT_TIMESTAMP,
  `IP` varchar(20) NOT NULL,
  `data` varchar(200) NOT NULL,
  PRIMARY KEY  (`log_id`),
  KEY `account_id` (`account_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `harmony_log`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `loginlog`
--

CREATE TABLE IF NOT EXISTS `loginlog` (
  `time` datetime NOT NULL default '0000-00-00 00:00:00',
  `ip` varchar(15) NOT NULL default '',
  `user` varchar(23) NOT NULL default '',
  `rcode` tinyint(4) NOT NULL default '0',
  `log` varchar(255) NOT NULL default '',
  `mac` varchar(18) NOT NULL default '',
  KEY `ip` (`ip`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Extraindo dados da tabela `loginlog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `mvplog`
--

CREATE TABLE IF NOT EXISTS `mvplog` (
  `mvp_id` mediumint(9) unsigned NOT NULL auto_increment,
  `mvp_date` datetime NOT NULL default '0000-00-00 00:00:00',
  `kill_char_id` int(11) NOT NULL default '0',
  `monster_id` smallint(6) NOT NULL default '0',
  `prize` int(11) NOT NULL default '0',
  `mvpexp` mediumint(9) NOT NULL default '0',
  `map` varchar(11) NOT NULL default '',
  PRIMARY KEY  (`mvp_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `mvplog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `npclog`
--

CREATE TABLE IF NOT EXISTS `npclog` (
  `npc_id` mediumint(9) unsigned NOT NULL auto_increment,
  `npc_date` datetime NOT NULL default '0000-00-00 00:00:00',
  `account_id` int(11) unsigned NOT NULL default '0',
  `char_id` int(11) unsigned NOT NULL default '0',
  `char_name` varchar(25) NOT NULL default '',
  `map` varchar(11) NOT NULL default '',
  `mes` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`npc_id`),
  KEY `account_id` (`account_id`),
  KEY `char_id` (`char_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `npclog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `picklog`
--

CREATE TABLE IF NOT EXISTS `picklog` (
  `id` int(11) NOT NULL auto_increment,
  `time` datetime NOT NULL default '0000-00-00 00:00:00',
  `account_id` int(11) NOT NULL default '0',
  `char_id` int(11) NOT NULL default '0',
  `name` varchar(25) NOT NULL,
  `type` enum('M','P','L','T','V','S','N','C','A','R','G','E','B','I') NOT NULL default 'P',
  `nameid` int(11) NOT NULL default '0',
  `amount` int(11) NOT NULL default '1',
  `refine` tinyint(3) unsigned NOT NULL default '0',
  `card0` int(11) NOT NULL default '0',
  `card1` int(11) NOT NULL default '0',
  `card2` int(11) NOT NULL default '0',
  `card3` int(11) NOT NULL default '0',
  `map` varchar(11) NOT NULL default '',
  `serial` int(11) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `type` (`type`),
  KEY `account_id` (`account_id`),
  KEY `char_id` (`char_id`),
  KEY `nameid` (`nameid`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `picklog`
--


-- --------------------------------------------------------

--
-- Estrutura da tabela `zenylog`
--

CREATE TABLE IF NOT EXISTS `zenylog` (
  `id` int(11) NOT NULL auto_increment,
  `time` datetime NOT NULL default '0000-00-00 00:00:00',
  `char_id` int(11) NOT NULL default '0',
  `src_id` int(11) NOT NULL default '0',
  `type` enum('M','T','V','S','N','A','E','B') NOT NULL default 'S',
  `amount` int(11) NOT NULL default '0',
  `map` varchar(11) NOT NULL default '',
  PRIMARY KEY  (`id`),
  KEY `type` (`type`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

--
-- Extraindo dados da tabela `zenylog`
--

