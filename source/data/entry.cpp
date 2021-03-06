﻿/* This is simple C++ program, working as launcher for Ü program.
  Usage - wrote your Ü program, compile it into set of object files, compile it against this launcher.
  For example:
    gcc my_ü_program.o entry.cpp -o my_ü_program.exe

  This launcher developed for usage of Ü together with some C++ code. Launcher allows you to use C++ standart library,
  which initialized normally before C++ "main" function.
*/
#include <csignal>
#include <cstdlib>
#include <iostream>

// Your Ü entry point.
extern int U_Main();

// Ü halt handler. You can set it to your own handler.
// Do it before any Ü code execution.
extern void (*_U_halt_handler)();

static void HaltHandler()
{
	std::cout << "Ü programm error - \"halt\" executed" << std::endl;
	std::exit(-1);
}

static void SigFpeHandler(int)
{
	// On x86-64 linux SIGFPE raises on integer division by zero.
	std::cout << "Ü programm error - \"SIGFPE\"" << std::endl;
	std::exit(-1);
}

int main()
{
	_U_halt_handler= HaltHandler;
	std::signal( SIGFPE, SigFpeHandler );
	U_Main();
}
