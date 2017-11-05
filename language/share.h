
#include "package.h"
#include "depend.h"

#ifndef HACKSAW_COMPILER_SHARE_H
#define HACKSAW_COMPILER_SHARE_H

extern int indent;
void printf_with_indent (char* format, ...);
extern int error;

PackageSelector* package_parse (char* buffer);
DependExpression* depend_parse (char* buffer);

#endif // HACKSAW_COMPILER_SHARE_H