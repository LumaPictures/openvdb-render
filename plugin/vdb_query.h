#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class VDBQueryCmd : public MPxCommand {
public:
    VDBQueryCmd();
    ~VDBQueryCmd();

    static void* creator();
    static MSyntax create_syntax();

    MStatus doIt(const MArgList& args);
};
