/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef __pbxbuild_WorkspaceContext_h
#define __pbxbuild_WorkspaceContext_h

#include <pbxbuild/Base.h>

namespace pbxbuild {

/*
 * Abstracts a loaded workspace. Workspaces can be an actual workspace file,
 * or in legacy builds, from just a project file. Workspaces define:
 *
 *  - The available schemes (from inside a .xcworkspace or a .pbxproj).
 *  - All of the available projects (including nested projects).
 *  - The build root, where workspace-relative paths are based.
 *  - Where outputs go in derived data (i.e. OBJROOT).
 */
class WorkspaceContext {
private:
    std::string                                   _basePath;
    std::string                                   _derivedDataName;
    xcworkspace::XC::Workspace::shared_ptr        _workspace;
    pbxproj::PBX::Project::shared_ptr             _project;
    std::vector<xcscheme::XC::Scheme::shared_ptr> _schemes;
    // TODO(grp): Mutable until nested projects are loaded when the workspace is created.
    mutable std::unordered_map<std::string, pbxproj::PBX::Project::shared_ptr> _projects;

public:
    WorkspaceContext(
        std::string const &basePath,
        std::string const &derivedDataName,
        xcworkspace::XC::Workspace::shared_ptr const &workspace,
        pbxproj::PBX::Project::shared_ptr const &project,
        std::vector<xcscheme::XC::Scheme::shared_ptr> const &schemes,
        std::unordered_map<std::string, pbxproj::PBX::Project::shared_ptr> const &projects);
    ~WorkspaceContext();

public:
    /*
     * The base path of either the .xcworkspace or the legacy .xcodeproj.
     */
    std::string const &basePath() const
    { return _basePath; }

    /*
     * The path inside DerivedData to use when building this workspace.
     */
    std::string const &derivedDataName() const
    { return _derivedDataName; }

public:
    /*
     * The workspace. May be nullptr if this is a legacy project-only workspace.
     */
    xcworkspace::XC::Workspace::shared_ptr const &workspace() const
    { return _workspace; }

    /*
     * The root project. May be nullptr if this is a real workspace.
     */
    pbxproj::PBX::Project::shared_ptr const &project() const
    { return _project; }

public:
    /*
     * All schemes from the workspace itself and any projects in the workspace.
     */
    std::vector<xcscheme::XC::Scheme::shared_ptr> const &schemes() const
    { return _schemes; }

    /*
     * All projects, including the root project, workspace projects, and nested projects.
     */
    std::unordered_map<std::string, pbxproj::PBX::Project::shared_ptr> const &projects() const
    { return _projects; }

public:
    /*
     * Find a project in the workspace. Could be the root project (for a legacy build), a project
     * referenced by the workspace (with a real workspace), or a nested project in any of those.
     * project referenced by the workspace.
     */
    pbxproj::PBX::Project::shared_ptr
    project(std::string const &projectPath) const;

    /*
     * Find a scheme from inside the workspace. For real workspaces, schemes
     * are searched in the workspace itself as well as projects in the workspace.
     */
    xcscheme::XC::Scheme::shared_ptr
    scheme(std::string const &name) const;

public:
    /*
     * Creates a workspace context from a real workspace.
     */
    static WorkspaceContext
    Workspace(xcworkspace::XC::Workspace::shared_ptr const &workspace);

    /*
     * Creates a workspace context for a legacy project-only build.
     */
    static WorkspaceContext
    Project(pbxproj::PBX::Project::shared_ptr const &project);
};

}

#endif // !__pbxbuild_WorkspaceContext_h
