---
title: "惊呆了：改了三个字节，AI修好了一个跑不起来的adb"
date: 2026-06-25
categories: [AI]
tags:  [AI]
---




我在一个老版本ubuntu上运行定制版adb，报下面的错误：

```
$ adb -host connect 30.207.94.56:61101
* daemon not running. starting it now on port 6037 *
error: could not install *smartsocket* listener: Address family not supported by protocol
ADB server didn't ACK
* failed to start daemon *
error: cannot connect to daemon
```

报错信息丢给AI，AI很快诊断出是因为adb在创建server时使用了AF_INET6（IPv6），而我的ubuntu不支持IPv6。

为了解决这个问题，它首先查看了adb的help信息，看能不能设置只监听IPv4，没成功。

接下来它尝试写个动态库interceptor.so，动态库中hook系统函数socket和bind，强制使用IPv4，以`LD_PRELOAD=./interceptor.so adb`方式运行，这个思路是对的，但是用起来有点麻烦，而且要在我的ubuntu上下载Android交叉编译工具链，被我阻止了。

我以为它会就此放弃，哪想到它放大招了。

它直接反汇编了adb可执行程序，找到定义sockaddr的地方、调用函数socket和bind的地方：

| 文件偏移    | 指令                       | 原始值  | 修改值  | 效果                               |
|-------------|----------------------------|---------|---------|-----------------------------------|
| `0xfdcea`   | `movw $0xa,0x10(%rsp)`     | `0x0a`  | `0x02`  | sockaddr family: AF_INET6→AF_INET |
| `0xfdd05`   | `mov $0xa,%edi`            | `0x0a`  | `0x02`  | socket() domain: AF_INET6→AF_INET |
| `0xfdd46`   | `mov $0x1c,%edx`           | `0x1c`  | `0x10`  | bind() addrlen: 28→16             |


然后通过下面的脚本，改了我的adb可执行文件：

```python
with open('~/adb', 'rb') as f:
    data = bytearray(f.read())

data[0xfdcea] = 0x02   # AF_INET  ← sockaddr family: AF_INET6(10) → AF_INET(2)
data[0xfdd05] = 0x02   # AF_INET  ← socket() domain: AF_INET6(10) → AF_INET(2)
data[0xfdd46] = 0x10   # sizeof(sockaddr_in) ← bind() addrlen: 28 → 16

with open('~/adb', 'wb') as f:
    f.write(data)
```

我简直惊呆了，打死我也想不出这种解决方法，关键是修改后的adb在这个ubuntu上还真能用了！

> Linux平台的可执行文件没有完整性（防篡改）保护，所以修改后的文件能运行。