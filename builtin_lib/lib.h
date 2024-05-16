#ifndef KALEI_LIB_H
#define KALEI_LIB_H

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT double putchard(double x);
extern "C" DLLEXPORT double tab();
extern "C" DLLEXPORT double endl();
extern "C" DLLEXPORT double printNum(double x);
#endif