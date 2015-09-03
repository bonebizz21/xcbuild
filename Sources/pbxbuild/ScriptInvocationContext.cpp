// Copyright 2013-present Facebook. All Rights Reserved.

#include <pbxbuild/ScriptInvocationContext.h>
#include <pbxbuild/ToolInvocationContext.h>
#include <pbxbuild/FileTypeResolver.h>

using pbxbuild::ScriptInvocationContext;
using pbxbuild::ToolInvocationContext;
using ToolEnvironment = pbxbuild::ToolInvocationContext::ToolEnvironment;
using OptionsResult = pbxbuild::ToolInvocationContext::OptionsResult;
using CommandLineResult = pbxbuild::ToolInvocationContext::CommandLineResult;
using pbxbuild::ToolInvocation;
using pbxbuild::FileTypeResolver;
using libutil::FSUtil;

ScriptInvocationContext::
ScriptInvocationContext(ToolInvocation const &invocation) :
    _invocation(invocation)
{
}

ScriptInvocationContext::
~ScriptInvocationContext()
{
}

static pbxsetting::Level
ScriptInputOutputLevel(std::vector<std::string> const &inputFiles, std::vector<std::string> const &outputFiles, bool multipleInputs)
{
    std::vector<pbxsetting::Setting> settings;

    settings.push_back(pbxsetting::Setting::Parse("SCRIPT_OUTPUT_FILE_COUNT", std::to_string(outputFiles.size())));
    for (auto it = outputFiles.begin(); it < outputFiles.end(); ++it) {
        size_t index = (it - outputFiles.begin());
        settings.push_back(pbxsetting::Setting::Parse("SCRIPT_OUTPUT_FILE_" + std::to_string(index), *it));
    }

    if (multipleInputs) {
        settings.push_back(pbxsetting::Setting::Parse("SCRIPT_INPUT_FILE_COUNT", std::to_string(inputFiles.size())));
        for (auto it = inputFiles.begin(); it < inputFiles.end(); ++it) {
            size_t index = (it - inputFiles.begin());
            settings.push_back(pbxsetting::Setting::Parse("SCRIPT_INPUT_FILE_" + std::to_string(index), *it));
        }
    } else if (!inputFiles.empty()) {
        settings.push_back(pbxsetting::Setting::Parse("SCRIPT_INPUT_FILE", inputFiles.front()));
    }

    return pbxsetting::Level(settings);
}

ScriptInvocationContext ScriptInvocationContext::
Create(
    pbxspec::PBX::Tool::shared_ptr scriptTool,
    std::string const &script,
    std::string const &scriptPath,
    std::string const &shell,
    bool multipleInputs,
    std::vector<std::string> const &inputFiles,
    std::vector<std::string> const &outputFiles,
    pbxsetting::Environment const &environment,
    std::string const &workingDirectory,
    std::string const &logMessage
)
{
    pbxsetting::Level level = ScriptInputOutputLevel(inputFiles, outputFiles, multipleInputs);
    std::vector<pbxsetting::Level> levels = environment.assignment();
    levels.insert(levels.begin(), level);
    pbxsetting::Environment scriptEnvironment = pbxsetting::Environment(levels, levels);

    std::unordered_map<std::string, std::string> values = scriptEnvironment.computeValues(pbxsetting::Condition::Empty());
    std::map<std::string, std::string> environmentVariables = std::map<std::string, std::string>(values.begin(), values.end());

    std::string scriptArgument;
    std::string scriptContents;
    if (!scriptPath.empty()) {
        scriptArgument = scriptPath;
        scriptContents = (!shell.empty() ? "#!" + shell + "\n" + script : script);
    } else {
        scriptArgument = script;
        scriptContents = "";
    }

    ToolEnvironment toolEnvironment = ToolEnvironment::Create(scriptTool, scriptEnvironment, inputFiles, outputFiles);
    OptionsResult options = OptionsResult::Create(toolEnvironment, environmentVariables);
    CommandLineResult commandLine = CommandLineResult::Create(toolEnvironment, options, "/bin/sh", { "-c", scriptArgument });
    ToolInvocationContext context = ToolInvocationContext::Create(toolEnvironment, options, commandLine, logMessage, workingDirectory, "", scriptPath, scriptContents);
    return ScriptInvocationContext(context.invocation());
}

ScriptInvocationContext ScriptInvocationContext::
Create(
    pbxspec::PBX::Tool::shared_ptr scriptTool,
    pbxproj::PBX::ShellScriptBuildPhase::shared_ptr const &buildPhase,
    pbxsetting::Environment const &environment,
    std::string const &workingDirectory
)
{
    pbxsetting::Level level = pbxsetting::Level({
        pbxsetting::Setting::Parse("BuildPhaseName", (!buildPhase->name().empty() ? buildPhase->name() : "Run Script")),
        pbxsetting::Setting::Parse("BuildPhaseIdentifier", buildPhase->blueprintIdentifier()),
    });

    std::vector<pbxsetting::Level> levels = environment.assignment();
    levels.insert(levels.begin(), level);
    pbxsetting::Environment phaseEnvironment = pbxsetting::Environment(levels, levels);

    pbxsetting::Value scriptPath = pbxsetting::Value::Parse("$(TEMP_FILES_DIR)/Script-$(BuildPhaseIdentifier).sh");
    pbxsetting::Value logMessage = pbxsetting::Value::Parse("PhaseScriptExecution $(BuildPhaseName:quote) ") + scriptPath;

    std::vector<std::string> inputFiles;
    std::transform(buildPhase->inputPaths().begin(), buildPhase->inputPaths().end(), std::back_inserter(inputFiles), [&](pbxsetting::Value const &input) -> std::string {
        return environment.expand(input);
    });

    std::vector<std::string> outputFiles;
    std::transform(buildPhase->outputPaths().begin(), buildPhase->outputPaths().end(), std::back_inserter(outputFiles), [&](pbxsetting::Value const &output) -> std::string {
        return environment.expand(output);
    });

    // TODO(grp): Use PBX::ShellScriptBuildPhase::showEnvVarsInLog().
    return Create(
        scriptTool,
        buildPhase->shellScript(),
        phaseEnvironment.expand(scriptPath),
        buildPhase->shellPath(),
        true,
        inputFiles,
        outputFiles,
        environment,
        workingDirectory,
        phaseEnvironment.expand(logMessage)
    );
}

ScriptInvocationContext ScriptInvocationContext::
Create(
    pbxspec::PBX::Tool::shared_ptr scriptTool,
    std::string const &inputFile,
    pbxbuild::TargetBuildRules::BuildRule::shared_ptr const &buildRule,
    pbxsetting::Environment const &environment,
    std::string const &workingDirectory
)
{
    pbxsetting::Level level = pbxsetting::Level({
        pbxsetting::Setting::Parse("INPUT_FILE_DIR", FSUtil::GetDirectoryName(inputFile)),
        pbxsetting::Setting::Parse("INPUT_FILE_BASE", FSUtil::GetBaseNameWithoutExtension(inputFile)),
        pbxsetting::Setting::Parse("INPUT_FILE_NAME", FSUtil::GetBaseName(inputFile)),
        pbxsetting::Setting::Parse("INPUT_FILE_PATH", inputFile),
        pbxsetting::Setting::Parse("INPUT_FILE_SUFFIX", (!FSUtil::GetFileExtension(inputFile).empty() ? "." + FSUtil::GetFileExtension(inputFile) : "")),
        // TODO(grp): INPUT_FILE_REGION_PATH_COMPONENT
    });

    std::vector<pbxsetting::Level> levels = environment.assignment();
    levels.insert(levels.begin(), level);
    pbxsetting::Environment ruleEnvironment = pbxsetting::Environment(levels, levels);

    pbxsetting::Value logMessage = pbxsetting::Value::Parse("RuleScriptExecution " + FSUtil::GetRelativePath(inputFile, workingDirectory) + " $(variant) $(arch)");

    std::vector<std::string> outputFiles;
    std::transform(buildRule->outputFiles().begin(), buildRule->outputFiles().end(), std::back_inserter(outputFiles), [&](pbxsetting::Value const &output) -> std::string {
        return ruleEnvironment.expand(output);
    });

    return Create(
        scriptTool,
        buildRule->script(),
        "",
        "",
        false,
        { inputFile },
        outputFiles,
        ruleEnvironment,
        workingDirectory,
        ruleEnvironment.expand(logMessage)
    );
}

