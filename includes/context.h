#pragma once
#include <memory>
#include <string>
#include <fstream>

#define DEBUG


inline std::unique_ptr<std::ifstream> sourceInput;

extern void SettingContext(std::string);
extern void MainLoop();
extern void ContextClose();
