---
title: "Json­-C用法释疑"
date: 2017-04-08
categories: [开源软件]
tags: [json-c]
---

实际项目中发现Json-C用法不当导致的内存泄露、踩内存问题，大都是因为不清楚下面几个接口的用法。  
以下分析基于https://github.com/json-c/json-c（ 0.12.1 release）。


# 1. json_object_new_object生成的对象要不要释放 #
```c
int main(int argc, char **argv)
{
	struct json_object* obj;
	mtrace();
	obj = json_object_new_object();
	//json_object_put(obj);
	return 0;
}  
```
上面的代码执行后，你会发现泄漏下面这些内存：
```console
Memory not freed:
-----------------
           Address     Size     Caller
0x0000000000b6a460     0x48  at json-c-json-c-0.12.1-20160607/json_object.c:185
0x0000000000b6a4b0     0x58  at json-c-json-c-0.12.1-20160607/linkhash.c:435
0x0000000000b6a510    0x200  at json-c-json-c-0.12.1-20160607/linkhash.c:440
```
所以，**json_object_new_object生成的对象必须调用json_object_put释放**。  
# 2. json_tokener_parse生成的对象要不要释放 #

```c
int main(int argc, char **argv)
{
    mtrace();
    const char *str = "{\"a\":1}";
    struct json_object* obj = json_tokener_parse(str);
    //json_object_put(obj);
    return 0;
}
```
上面这些代码执行后，你会发现下面这些 内存泄漏：

```console
Memory not freed:
-----------------
           Address     Size     Caller
0x00000000022e7930     0x48  at json-c-json-c-0.12.1-20160607/json_object.c:185
0x00000000022e7980     0x58  at json-c-json-c-0.12.1-20160607/linkhash.c:435
0x00000000022e79e0    0x200  at json-c-json-c-0.12.1-20160607/linkhash.c:440
0x00000000022e7c10     0x48  at json-c-json-c-0.12.1-20160607/json_object.c:185
```
所以，**json_tokener_parse生成的对象，必须使用json_object_put释放.**  
# 3. json_object_object_get出来的对象要不要释放 #

```c
int main(int argc, char **argv)
{
    struct json_object* obj;
    struct json_object *child;
     
    obj = json_object_new_object();
     
    json_object_object_add(obj, "a", json_object_new_int(1));
    json_object_object_add(obj, "b", json_object_new_int(2));
     
    child = json_object_object_get(obj,"a");
    json_object_put(child);     //Oh, No!!!
    json_object_put(obj);
    return 0;
}
```
借助内存越界检测工具efence和gdb，运行代码发现段错误，其中test.c:22指向json_object_put(obj)这一行.  
这是因为child节点被释放过了，现在又去释放， 使用了野指针（不借助工具，程序会正常结束，这也是这种错误的可怕之处）。  
这种不会立即终止程序的错误太可怕 ，让你都不知道怎么死的。

```console
Program received signal SIGSEGV, Segmentation fault.
json_object_put (jso=0x7ffff7ee2fb8) at json_object.c:154
154                     jso->_ref_count--;
(gdb) bt
#0  json_object_put (jso=0x7ffff7ee2fb8) at json_object.c:154
#1  0x0000000000403346 in lh_table_free (t=0x7ffff7edefa8) at linkhash.c:485
#2  0x000000000040190d in json_object_object_delete (jso=0x7ffff7edcfb8) at json_object.c:354
#3  0x0000000000401edd in json_object_put (jso=0x7ffff7edcfb8) at json_object.c:159
#4  json_object_put (jso=0x7ffff7edcfb8) at json_object.c:150
#5  0x0000000000401515 in main (argc=1, argv=0x7fffffffdfd8) at test.c:22
```
所以，**通过json_object_object_get获取的对象不能单独释放，因为它仍然归父节点所有。**
# 4. 通过json_object_object_add添加到其他节点的，能不能释放 #

```c
int main(int argc, char **argv)
{
    struct json_object* obj;
    struct json_object *child;
    
    child = json_object_new_object();

    obj = json_object_new_object();
    json_object_object_add(obj, "a", json_object_new_int(1));
    json_object_object_add(obj, "child", child);
     
    json_object_put(child);     //Oh, No!!!
    json_object_put(obj);
    return 0;
}
```

这个运行后，产生的错误和3中类似，也是因为重复释放。
所以，**通过json_object_object_add添加到其他节点的不能再单独释放，因为他已经成为别人的子节点，他的生命周期由父节点维护了。**
# 5. json_object_to_json_string获取到的字串要不要释放 #

```c
int main(int argc, char **argv)
{
    struct json_object* obj;
    char *str;
    obj = json_object_new_object();
    json_object_object_add(obj, "a", json_object_new_int(1));
    json_object_object_add(obj, "b", json_object_new_int(2));
    str =  json_object_to_json_string(obj);
     
    free(str);     //Oh, No!!!
    json_object_put(obj);
    return 0;
}
```
这个free也是非法的，因为**json_object_to_json_string只是把json对象内部的指针暴露给你了，借你用下而已，千万别释放。**
# 6. Other #
上面这几点疑惑，通过API接口描述文档都可以消除掉，再不济看看作者的Demo、源码也可以消除掉。  
所以，大家使用开源软件时，一定要搞明白再用，否则会带来很多问题。

