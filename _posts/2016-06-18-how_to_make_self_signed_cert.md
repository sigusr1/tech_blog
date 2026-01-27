---
title: "如何制作自签名证书"
date: 2016-06-18  
categories: [网络]
tags: [安全, 证书]  
---

本文主要介绍如何基于openssl制作X.509自签名证书，以及如何使用该证书签发新证书。


## 一、生成根证书 ##

### 1. 生成根证书私钥（pem文件） ###

下面的命令用来生成根证书的私钥，相关参数说明如下：  

- genrsa：使用RSA算法产生私钥，尾部的2048代表密钥长度为2048比特。
- aes256：使用256位密钥的AES算法对私钥进行加密，也可以选择其他算法进行加密。若使用加密，输入指令后会提示用户输入密码，该密码需妥善保管，因为后续只要使用该私钥都需要输入密码；如果不想对私钥加密，不使用该选项即可。
- out：输出的私钥文件名，可以指定路径。  

```console
openssl genrsa -aes256 -out rootCA.pem 2048
```

### 2. 生成根证书签发申请文件（csr文件） ###

下面的命令，使用上面生成的私钥，生成证书申请文件，相关参数说明如下：  

- req：请求命令
- new：新证书签发请求
- key：生成证书所使用的私钥文件
- out：输出的证书签发申请文件名

下面的指令输入后，会提示用户输入一些信息，如国家（中国是CN）/省份/城市/公司等，根据提示输入即可，若对应的项不填写，可以输入`.`。  
注意：  

- `Common Name`中可以输入该证书对应的域名。
- 签发的子证书中`Common Name`必须和根证书的不同，最好相互之间也不重复。  

```console
openssl req -new -key rootCA.pem -out rootCA.csr
```

### 3. 生成根证书（cer文件） ###

证书签发申请文件(csr文件）生成后，可以发送给CA机构，让其帮忙签发证书（一般是收费的），也可以使用下面的命令生成自签名证书，相关参数说明如下：  

- x509：证书格式为X.509
- req：请求命令
- days：证书的有效期，单位是天
- sha1：证书摘要采用sha1算法
- signkey：签发证书使用的私钥
- in：证书签发申请文件（csr文件）
- out：输出的cer证书文件

```console
openssl x509 -req -days 365 -sha1 -signkey rootCA.pem -in rootCA.csr -out rootCA.cer
```

至此，我们已经拥有了根证书rootCA.cer，以及该证书对应的私钥rootCA.pem。  

*注：公钥可以根据私钥生成，后续用不到，所以不再生成。*

## 二、签发Server端证书 ##

1. 生成Sever端的私钥

	```console
	openssl genrsa -out server.pem 2048  
	```

2. 生成证书签发申请文件  
输入下面的指令后，根据提示输入相关信息：

	```console
	openssl req -new -key server.pem -out server.csr
	```
3. 使用根证书签发Server端证书  
在颁发Server端证书的时候，用到了根证书rootCA.cer，以及根证书对应的私钥rootCA.pem，二者缺一不可。部分参数说明如下：
- CA：证书颁发机构的证书，这里是根证书rootCA.cer，多级签发的时候，这里也可以是中间证书
- CAkey：证书颁发机构的的私钥，这里是根证书的私钥rootCA.pem，多级签发的时候，这里也可以是中间证书的私钥
- CAcreateserial：创建证书序列号文件，该序列号在经由rootCA颁发的证书中是全局唯一的，可以唯一标识一个证书；创建的序列号文件默认名称为`CA参数指定的证书名加上.srl后缀`，比如下面的例子生成的序列号文件为rootCA.srl。

	```console
	openssl x509 -req -days 365 -sha1  -CA rootCA.cer -CAkey rootCA.pem -in server.csr -CAcreateserial -out server.cer
	```

现在我们拥有了Server端的证书server.cer以及对应的私钥server.pem。

## 三、签发Client端证书 ##

1. 生成Client端的私钥
```console
	openssl genrsa -out client.pem 2048  
```

2. 生成证书签发申请文件  
输入下面的指令后，根据提示输入相关信息：
```console
	openssl req -new -key client.pem -out client.csr
```
3. 使用根证书签发Client端证书
```console
openssl x509 -req -days 365 -sha1  -CA rootCA.cer -CAkey rootCA.pem -in client.csr -CAcreateserial -out client.cer
```
现在我们拥有了Client端的证书client.cer以及对应的私钥client.pem。

## 四、证书验证 ##

上面我们已经生成了根证书rootCA.cer，并用该根证书签发了服务器证书server.cer和客户端证书client.cer。
也就是说形成了两条信任链：

	rootCA.cer --> server.cer
	rootCA.cer --> client.cer

### 1. 使用openssl验证 ###

基于rootCA.cer验证sever.cer:

```console
openssl verify -CAfile rootCA.cer sever.cer 
```
成功则输出结果为OK，否则会有提示信息。


### 2. 使用Windows验证 ###

双击sever.cer，可以看到证书验证失败，这是因为系统上没人为该证书背书：  

![验证失败](/assets/images/2016-06-18-how_to_make_self_signed_cert/sever_not_auth.png)

双击rootCA.cer，点击下面的`安装证书`按钮，根据提示安装根证书（安装位置选择`安装到受信任的根证书颁发机构`）。根证书安装后，代表这台电脑无条件信任根证书，以及经由rootCA.cer签发的其他证书。  

这时再打开sever.cer，如下图所示，已经验证成功（有rootCA.cer为他进行信任背书了):  

![验证成功](/assets/images/2016-06-18-how_to_make_self_signed_cert/sever_auth.png)


## 五、参考文档 ##
1. [OpenSSL生成根证书CA及签发子证书](https://yq.aliyun.com/articles/40398?spm=a2c4e.11153940.0.0.69928c988yuZJK&type=2)
2. [OpenSSL - error 18 at 0 depth lookup:self signed certificate](https://stackoverflow.com/questions/19726138/openssl-error-18-at-0-depth-lookupself-signed-certificate)
3. [证书链-Digital Certificates](https://www.jianshu.com/p/46e48bc517d0)
4. [数字证书](https://blog.cnbluebox.com/blog/2014/03/24/shu-zi-zheng-shu/)