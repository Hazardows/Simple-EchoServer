@echo off

mkdir "bin" 2>NUL

echo Compilando servidor...
clang++ -o bin/echo_server.exe src/echo_server.cpp ^
  -Wno-vla-cxx-extension ^
  -lws2_32 ^
  -Xclang -MT

if %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

echo Compilando cliente...
clang++ -o bin/test_client.exe src/test_client.cpp ^
  -lws2_32 ^
  -Xclang -MT

if %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

echo Compilacion completada!