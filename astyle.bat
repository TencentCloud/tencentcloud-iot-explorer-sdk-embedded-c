echo off & color 0A
::指定起始文件夹
set DIR="%cd%"
echo DIR=%DIR%
 
:: 参数 /R 表示需要遍历子文件夹,去掉表示不遍历子文件夹
:: %%f 是一个变量,类似于迭代器,但是这个变量只能由一个字母组成,前面带上%%
:: 括号中是通配符,可以指定后缀名,*.*表示所有文件
for /R %DIR% %%f in (*.c) do ( 
C:\AStyle\bin\AStyle.exe --style=linux -s4 -S -p -N -L -H -m0 -M40 --suffix=none --convert-tabs %%f
)
pause