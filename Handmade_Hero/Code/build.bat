@echo off

mkdir ..\..\build
pushd ..\..\build
cl /Zi /std:c++17 ..\Handmade_Hero\Code\win32_handmade.cpp User32.lib gdi32.lib
popd
