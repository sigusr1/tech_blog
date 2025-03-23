---
title: " Source Insight集成svn命令"
date: 2015-05-31  
categories: [工具]
tags: [环境搭建, svn]
---

本文主要介绍如何在source insight上集成svn命令。

**1. SVN Log**  

option->Custom Commands   
添加一个命令  
名字：SVN Log  （随便自己写）  
运行："C:\Program Files\TortoiseSVN\bin\TortoiseProc.exe" /command:log /path:%f /notempfile /closeonend    
其中TortoiseProc.exe的目录以自己电脑上的为准。

**2. SVN Diff**  


option->Custom Commands  
添加一个命令  
名字：SVN Diff  （随便自己写）  
运行："C:\Program Files\TortoiseSVN\bin\TortoiseProc.exe" /command:diff /path:%f /notempfile /closeonend  

**同样，添加命令 ShellExecute open %d 可以打开当前文件所在文件夹。**

接下来可以在source insight上添加相关命令的自定义快捷键和自定义菜单。

                
