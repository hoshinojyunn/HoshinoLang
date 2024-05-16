#pragma once
#include <memory>
#include <string>
#include <fstream>

#define RELEASE


#ifdef RELEASE
inline std::unique_ptr<std::ifstream> sourceInput;
#endif

extern void SettingContext(std::string);
extern void MainLoop();
extern void ContextClose();
