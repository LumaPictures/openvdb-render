#include "vdb_query.h"

#include <iostream>
#include <limits>

#include <maya/MArgDatabase.h>
#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MTime.h>
#include <maya/MAnimControl.h>

#include "vdb_visualizer.h"

#include "vdb_maya_utils.hpp"

namespace {
    const char* file_short_flag = "f";
    const char* file_long_flag = "file";

    const char* node_short_flag = "n";
    const char* node_long_flag = "node";

    const char* grid_short_flag = "g";
    const char* grid_long_flag = "grid";

    const char* grid_type_short_flag = "gt";
    const char* grid_type_long_flag = "grid_type";

    const char* query_short_flag = "q";
    const char* query_long_flag = "query";

    const char* current_frame_short_flag = "cf";
    const char* current_frame_long_flag = "current_frame";

    const char* start_frame_short_flag = "sf";
    const char* start_frame_long_flag = "start_frame";

    const char* end_frame_short_flag = "ef";
    const char* end_frame_long_flag = "end_frame";

    const std::string query_type_bbox = "bbox";
    const std::string query_type_min_max = "minmax";

    void build_file_list(const std::string& path, int start_frame, int end_frame, std::vector<std::string>& out_paths)
    {
        if (boost::regex_match(path, VDBVisualizerShape::s_frame_expr))
        {
            if (end_frame < start_frame)
                return;
            size_t hash_count = 0;
            for (auto c : path)
            {
                if (c == '#')
                    ++hash_count;
            }
            out_paths.reserve(static_cast<size_t>(std::max(end_frame - start_frame + 1, 1)));
            for (int frame = start_frame; frame < end_frame; ++frame)
            {
                std::stringstream ss;
                ss.fill('0');
                ss.width(hash_count);
                ss << frame;
                out_paths.push_back(boost::regex_replace(path, VDBVisualizerShape::s_hash_expr, ss.str()));
            }
        }
        else
            out_paths.push_back(path);
    }
}

VDBQueryCmd::VDBQueryCmd()
{

}

VDBQueryCmd::~VDBQueryCmd()
{

}

void* VDBQueryCmd::creator()
{
    return new VDBQueryCmd();
}

MSyntax VDBQueryCmd::create_syntax()
{
    MSyntax syntax;
    syntax.addFlag(file_short_flag, file_long_flag, MSyntax::kString);
    syntax.addFlag(node_short_flag, node_long_flag, MSyntax::kSelectionItem);
    syntax.addFlag(grid_short_flag, grid_long_flag, MSyntax::kString);
    syntax.addFlag(grid_type_short_flag, grid_type_long_flag, MSyntax::kString);
    syntax.addFlag(query_short_flag, query_long_flag, MSyntax::kString);
    syntax.addFlag(current_frame_short_flag, current_frame_long_flag);
    syntax.addFlag(start_frame_short_flag, start_frame_long_flag, MSyntax::kLong);
    syntax.addFlag(end_frame_short_flag, end_frame_long_flag, MSyntax::kLong);

    return syntax;
}

MStatus VDBQueryCmd::doIt(const MArgList& args)
{
    MStatus status = MS::kSuccess;
    MArgDatabase arg_data(syntax(), args);

    // always open new files to simplify code, it's cheap anyhow
    std::vector<std::string> vdb_paths;

    if (arg_data.isFlagSet(node_short_flag))
    {
        MSelectionList slist;
        arg_data.getFlagArgument(node_short_flag, 0, slist);

        MObject node;
        slist.getDependNode(0, node);
        MFnDependencyNode dnode(node, &status);

        if (!status)
            return status;

        if (dnode.typeName() != VDBVisualizerShape::typeName)
        {
            MGlobal::displayError("[openvdb] Wrong node was passed to the command : " + dnode.name());
            return MS::kFailure;
        }

        if (arg_data.isFlagSet(current_frame_short_flag))
            vdb_paths.push_back(MPlug(node, VDBVisualizerShape::s_out_vdb_path).asString().asChar());
        else
        {
            build_file_list(MPlug(node, VDBVisualizerShape::s_vdb_path).asString().asChar(),
                            MPlug(node, VDBVisualizerShape::s_cache_playback_start).asInt(),
                            MPlug(node, VDBVisualizerShape::s_cache_playback_end).asInt(),
                            vdb_paths);
        }
    }
    else if (arg_data.isFlagSet(file_short_flag))
    {
        MString vdb_path;
        arg_data.getFlagArgument(file_short_flag, 0, vdb_path);
        if (arg_data.isFlagSet(current_frame_short_flag))
        {
            const int current_frame = static_cast<int>(MAnimControl::currentTime().as(MTime::uiUnit()));
            build_file_list(vdb_path.asChar(), current_frame, current_frame, vdb_paths);
        }
        else
        {
            int start_frame = 0;
            int end_frame = 0;
            if (arg_data.isFlagSet(start_frame_short_flag))
                arg_data.getFlagArgument(start_frame_short_flag, 0, start_frame);
            else
                start_frame = static_cast<int>(MAnimControl::animationStartTime().as(MTime::uiUnit()));

            if (arg_data.isFlagSet(end_frame_short_flag))
                arg_data.getFlagArgument(end_frame_short_flag, 0, end_frame);
            else
                end_frame = static_cast<int>(MAnimControl::animationEndTime().as(MTime::uiUnit()));

            build_file_list(vdb_path.asChar(), start_frame, end_frame, vdb_paths);
        }
    }
    else
    {
        MGlobal::displayError("[openvdb] No cache was passed to the command, use the -file(f) or the -node(n) flags");
        return MS::kFailure;
    }

    if (vdb_paths.size() == 0)
    {
        MGlobal::displayError("[openvdb] No paths are passed to the command.");
        return MS::kFailure;
    }

    std::vector<openvdb::io::File*> vdb_files;
    vdb_files.reserve(vdb_paths.size());

    MString query_type = "";
    if (arg_data.isFlagSet(query_short_flag))
        arg_data.getFlagArgument(query_short_flag, 0, query_type);
    else
    {
        MGlobal::displayError("[openvdb] No query is specified.");
        return MS::kFailure;
    }

    auto get_array_from_flag = [&](const char* flag_name, std::vector<std::string>& out_values) {
        if (arg_data.isFlagSet(flag_name))
        {
            MString flag_data;
            arg_data.getFlagArgument(flag_name, 0, flag_data);
            MStringArray flags;
            if (flag_data.index(','))
                flag_data.split(',', flags);
            else if (flag_data.index(';'))
                flag_data.split(';', flags);
            else if (flag_data.index(':'))
                flag_data.split(':', flags);
            else if (flag_data.index(' '))
                flag_data.split(' ', flags);
            else
                flags.append(flag_data);

            const unsigned int flag_count = flags.length();
            out_values.reserve(flag_count);
            for (unsigned int f = 0; f < flag_count; ++f)
                out_values.push_back(flags[f].asChar());
        }
    };

    std::vector<std::string> queries;
    get_array_from_flag(query_short_flag, queries);

    if (queries.size() == 0)
    {
        MGlobal::displayError("[openvdb] No queries are specified!");
        return MS::kFailure;
    }

    for (const auto& vdb_path : vdb_paths)
    {
        openvdb::io::File* vdb_file = new openvdb::io::File(vdb_path);
        vdb_file->open(false);
        if (vdb_file->isOpen())
            vdb_files.push_back(vdb_file);
        else
            delete vdb_file;
    }

    if (vdb_files.size() == 0)
    {
        MGlobal::displayError("[openvdb] No vdb files can be opened.");
        return MS::kFailure;
    }

    std::vector<std::string> grid_names;
    get_array_from_flag(grid_short_flag, grid_names);

    std::vector<std::string> grid_types;
    get_array_from_flag(grid_type_short_flag, grid_types);

    const bool all_grids = grid_names.size() == 0 && grid_types.size() == 0;

    auto grid_required = [&](openvdb::GridBase::ConstPtr grid) -> bool {
        if (all_grids)
            return true;
        else
        {
            return std::find(grid_names.begin(), grid_names.end(), grid->getName()) != grid_names.end() ||
                   std::find(grid_types.begin(), grid_types.end(), grid->valueType()) != grid_types.end();
        }
    };

    for (const auto& query : queries)
    {
        if (query == query_type_bbox)
        {
            MBoundingBox bbox;
            for (auto vdb_file : vdb_files)
            {
                openvdb::GridPtrVecPtr grids = vdb_file->readAllGridMetadata();
                for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                {
                    if (openvdb::GridBase::ConstPtr grid = *it)
                    {
                        if (grid_required(grid))
                            read_transformed_bounding_box(grid, bbox);
                    }
                }
            }
            const MPoint min = bbox.min();
            const MPoint max = bbox.max();
            appendToResult(min.x);
            appendToResult(min.y);
            appendToResult(min.z);
            appendToResult(max.x);
            appendToResult(max.y);
            appendToResult(max.z);
        }
        else if (query == query_type_min_max)
        {
            std::vector<double> mins;
            std::vector<double> maxs;
            for (auto vdb_file : vdb_files)
            {
                openvdb::GridPtrVecPtr grids = vdb_file->readAllGridMetadata();
                for (openvdb::GridPtrVec::const_iterator it = grids->begin(); it != grids->end(); ++it)
                {
                    if (openvdb::GridBase::ConstPtr grid = *it)
                    {
                        if (grid_required(grid))
                        {
                            // TODO: check for the minimum and maximum metadata
                            if (grid->valueType() == "float")
                            {
                                if (mins.size() < 1)
                                    mins.resize(1, std::numeric_limits<double>::max());
                                if (maxs.size() < 1)
                                    maxs.resize(1, std::numeric_limits<double>::min());

                                openvdb::FloatGrid::ConstPtr grid_data = openvdb::gridConstPtrCast<openvdb::FloatGrid>(vdb_file->readGrid(grid->getName()));

                                for (auto iter = grid_data->beginValueOn(); iter; ++iter)
                                {
                                    const double value = static_cast<double>(iter.getValue());
                                    mins[0] = std::min(mins[0], value);
                                    maxs[0] = std::max(maxs[0], value);
                                }
                            }
                            else if (grid->valueType() == "vec3s")
                            {
                                if (mins.size() < 3)
                                    mins.resize(3, std::numeric_limits<double>::max());
                                if (maxs.size() < 3)
                                    maxs.resize(3, std::numeric_limits<double>::min());

                                openvdb::Vec3SGrid::ConstPtr grid_data = openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(vdb_file->readGrid(grid->getName()));

                                for (auto iter = grid_data->beginValueOn(); iter; ++iter)
                                {
                                    const openvdb::Vec3d value = iter.getValue();
                                    mins[0] = std::min(mins[0], value.x());
                                    mins[1] = std::min(mins[1], value.y());
                                    mins[2] = std::min(mins[2], value.z());

                                    maxs[0] = std::max(maxs[0], value.x());
                                    maxs[1] = std::max(maxs[1], value.y());
                                    maxs[2] = std::max(maxs[2], value.z());
                                }
                            }
                        }
                    }
                }
            }
            for (const auto& mn : mins)
                appendToResult(mn);
            for (const auto& mx : maxs)
                appendToResult(mx);
        }
    }

    for (auto vdb_file : vdb_files)
        delete vdb_file;

    return status;
}
