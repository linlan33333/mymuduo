#!/bin/bash

# 报错信息全部显示
set -e      

# 如果没有build目录，则先创建build目录
if [ ! -d `pwd`/build ];
then
    mkdir `pwd`/build
fi

# 删除当前目录下的build目录下的文件
rm -rf `pwd`/build/*

# 自动化编译
cd `pwd`/build &&
    cmake .. &&
    make

# 回到项目根目录
cd ..

# 把头文件拷贝到 /usr/include/mymuduo  把so库拷贝到 /usr/lib
if [ ! -d /usr/include/mymuduo ];
then
    mkdir /usr/include/mymuduo
fi

cp `pwd`/lib/libmymuduo.so /usr/lib

for header in `ls *.h`
do
    cp $header /usr/include/mymuduo
done

# 别忘了刷新一下动态库缓存
ldconfig
