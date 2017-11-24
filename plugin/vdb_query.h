#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class VDBQueryCmd : public MPxCommand {
private:
    VDBQueryCmd() = default;
public:
    VDBQueryCmd(const VDBQueryCmd&) = delete;
    VDBQueryCmd(VDBQueryCmd&&) = delete;
    VDBQueryCmd& operator=(const VDBQueryCmd&) = delete;
    VDBQueryCmd& operator=(VDBQueryCmd&&) = delete;

    ~VDBQueryCmd() override = default;

    static void* creator();

    static MSyntax create_syntax();

    MStatus doIt(const MArgList& args) override;
};
