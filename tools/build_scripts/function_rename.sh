#! /bin/bash
# C-SDK函数和外部函数命名冲突时使用此脚本对C-SDK函数重命名
CUR_FOLDER=$(dirname "$0")

if [ $# != 2 ];then
    echo "USAGE: $0 old_name new_name"
    exit
fi

cd $CUR_FOLDER || exit
echo "Replace $1 with $2 in C-SDK"
sed -i "s/$1/$2/g" `grep -rwl $1 ../../*`
