
#include "Precomp.h"
#include "NativeCppUpdater.h"
#include "File.h"
#include "JsonValue.h"
#include "DebuggerApp.h"
#include <filesystem>
#include <stdexcept>

namespace
{
	std::string AutogeneratedStart = "//" "{AUTOGENERATED("; // Note: split into two strings to prevent the code below matching it!
	std::string AutogeneratedEnd = "//" "}AUTOGENERATED";
#ifdef WIN32
	std::string NewLineStr = "\r\n";
#else
	std::string NewLineStr = "\n";
#endif
}

void NativeCppUpdater::Run()
{
	if (!FindSourceCode())
	{
		Console->WriteOutput("Could not find SurrealEngine source code folder" + Console->NewLine());
		return;
	}

	ParseJsonFiles();

	// Update AUTOGENERATED sections:
	for (std::filesystem::path filename : std::filesystem::recursive_directory_iterator(SourceBasePath))
	{
		auto ext = filename.extension();
		if (ext != ".cpp" && ext != ".h")
			continue;

		std::string code = File::read_all_text(filename.string());
		std::string updatedcode;

		// Look for a codegen blocks and update them:
		size_t lastpos = 0;
		while (true)
		{
			// Find the next AUTOGENERATED line:
			size_t pos = std::min(code.find(AutogeneratedStart, lastpos), code.size());
			if (pos == code.size())
				break;

			size_t argsbegin = pos + AutogeneratedStart.size();
			size_t argsend = code.find(')', argsbegin);

			// Find the start and end of the first line:
			size_t startlinepos = pos;
			size_t startlineend = pos;
			while (startlinepos > 0 && code[startlinepos - 1] != '\n') startlinepos--;
			while (startlineend < code.size() && code[startlineend] != '\n') startlineend++;
			if (startlineend < code.size()) startlineend++;

			// Find the end of the AUTOGENERATED block:
			size_t endpos = std::min(code.find(AutogeneratedEnd, pos), code.size());

			// Find the start and end of the last line:
			size_t endlinepos = endpos;
			size_t endlineend = endpos;
			while (endlinepos > 0 && code[endlinepos - 1] != '\n') endlinepos--;
			while (endlineend < code.size() && code[endlineend] != '\n') endlineend++;
			if (endlineend < code.size()) endlineend++;

			// Invalid block
			if (endpos == code.size() || argsend > startlineend)
			{
				Console->WriteOutput("Invalid AUTOGENERATED block found in " + filename.string() + Console->NewLine());
				pos += AutogeneratedStart.size();
				updatedcode += code.substr(lastpos, pos - lastpos);
				lastpos = pos;
				continue;
			}

			std::string blockname = code.substr(argsbegin, argsend - argsbegin);
			std::string whitespaceprefix = code.substr(startlinepos, pos - startlinepos);
			std::string block = code.substr(startlineend, endlinepos - startlineend);

			updatedcode += code.substr(lastpos, startlinepos - lastpos);
			lastpos = endlineend;

			updatedcode += UpdateBlock(blockname, block, whitespaceprefix);
		}
		updatedcode += code.substr(lastpos);

		if (code != updatedcode)
		{
			Console->WriteOutput(filename.string() + Console->NewLine());
			//Console->WriteOutput(updatedcode);
			//Console->WriteOutput(Console->NewLine());
			File::write_all_text(filename.string(), updatedcode);
		}
	}
}

std::string NativeCppUpdater::UpdateBlock(const std::string& args, const std::string& block, const std::string& whitespaceprefix)
{
	size_t splitpos = std::min(args.find(','), args.size());
	std::string type = args.substr(0, splitpos);
	std::string name = args.substr(std::min(splitpos + 1, args.size()));

	std::string output = whitespaceprefix + AutogeneratedStart + args + ")" + NewLineStr;

	if (type == "Native") // Native header block (NObject.h, NActor.h, etc)
	{
		for (const NativeClass& cls : Classes)
		{
			if (cls.name != name)
				continue;

			for (const NativeFunction& func : cls.funcs)
			{
				for (int i = 0; i < func.decls.size(); i++)
				{
					const NativeFunctionDecl& decl = func.decls[i];
					std::string funcName = func.name;
					if (i > 0)
						funcName += "_" + std::to_string(i);

					output += whitespaceprefix + "static void " + funcName + "(" + decl.args + ");" + NewLineStr;
				}
			}
		}
	}
	else // Unknown block. Just let it be.
	{
		output += block;
	}
	
	output += whitespaceprefix + AutogeneratedEnd + NewLineStr;
	return output;
}

bool NativeCppUpdater::FindSourceCode()
{
	// Find the source code by assuming SurrealDebugger was launched from a build directory:
	std::filesystem::path sourceBase = std::filesystem::current_path();
	while (!std::filesystem::exists(sourceBase / "SurrealEngine"))
	{
		if (!sourceBase.has_parent_path())
			return false;
		sourceBase = sourceBase.parent_path();
	}
	SourceBasePath = sourceBase / "SurrealEngine";
	return true;
}

void NativeCppUpdater::ParseJsonFiles()
{
	for (std::string file : Directory::files(FilePath::combine(std::filesystem::current_path().string(), "*.json")))
	{
		size_t jsonTypePos = file.find("-Natives");
		bool parseNatives = true;
		if (jsonTypePos == std::string::npos)
		{
			parseNatives = false;
			jsonTypePos = file.find("-Properties");
			if (jsonTypePos == std::string::npos)
				continue;
		}

		size_t dashPos = file.find("-");
		std::string game = file.substr(0, dashPos);
		std::string version = file.substr(dashPos + 1, jsonTypePos - dashPos - 1);

		JsonValue json = JsonValue::parse(File::read_all_text(file));

		if (parseNatives)
			ParseGameNatives(json, version);
		else
			ParseGameProperties(json, version);
	}
}

void NativeCppUpdater::ParseGameNatives(const JsonValue& json, const std::string& version)
{
	const std::map<std::string, JsonValue>& props = json.properties();
	for (auto prop = props.begin(); prop != props.end(); prop++)
	{
		const JsonValue& package = (*prop).second;
		for (auto cls : package.properties())
		{
			ParseClassNatives(cls.first, (*prop).first, cls.second, version);
		}
	}
}

void NativeCppUpdater::ParseClassNatives(const std::string& className, const std::string& packageName, const JsonValue& json, const std::string& version)
{
	NativeClass& cls = AddUniqueNativeClass(className);

	// Hopefully we never run into this scenario :)
	// If we do, we'll have to figure out a way to address this
	if (cls.package.size() > 0 && cls.package != packageName)
		throw std::runtime_error("Class package mismatch between games, got " + cls.package + "first, then " + packageName);

	cls.package = packageName;

	const std::map<std::string, JsonValue>& props = json.properties();
	for (auto prop = props.begin(); prop != props.end(); prop++)
	{
		const std::string& funcName = (*prop).first;
		const JsonValue& funcJson = (*prop).second;
		cls.ParseClassFunction(funcName, funcJson, version);
	}
}

void NativeCppUpdater::ParseGameProperties(const JsonValue& json, const std::string& version)
{
	const std::map<std::string, JsonValue>& props = json.properties();
	for (auto prop = props.begin(); prop != props.end(); prop++)
	{
		const JsonValue& package = (*prop).second;
		for (auto cls : package.properties())
		{
			ParseClassProperties(cls.first, (*prop).first, cls.second, version);
		}
	}
}

void NativeCppUpdater::ParseClassProperties(const std::string& className, const std::string& packageName, const JsonValue& json, const std::string& version)
{
	NativeClass& cls = AddUniqueNativeClass(className);

	// Hopefully we never run into this scenario :)
	// If we do, we'll have to figure out a way to address this
	if (cls.package.size() > 0 && cls.package != packageName)
		throw std::runtime_error("Class package mismatch between games, got " + cls.package + "first, then " + packageName);

	cls.package = packageName;

	const std::map<std::string, JsonValue>& props = json.properties();
	for (auto prop = props.begin(); prop != props.end(); prop++)
	{
		NativeProperty& nativeProp = cls.AddUniqueNativeProperty((*prop).first);
		nativeProp.type = (*prop).second.to_string();
		nativeProp.games.push_back(version);
	}
}

NativeCppUpdater::NativeClass& NativeCppUpdater::AddUniqueNativeClass(const std::string& className)
{
	for (int i = 0; i < Classes.size(); i++)
	{
		NativeClass& cls = Classes[i];
		if (className == cls.name)
			return cls;
	}

	NativeClass cls;
	cls.name = className;
	Classes.push_back(std::move(cls));
	return Classes.back();
}

void NativeCppUpdater::NativeClass::ParseClassFunction(const std::string& funcName, const JsonValue& json, const std::string& version)
{
	std::string funcArgs = "UObject* Self";
	const std::vector<JsonValue>& args = json["Arguments"].items();
	if (args.size() > 0)
	{
		// Assemble function arguments
		for (auto arg : args)
		{
			funcArgs += ", " + arg.to_string();
		}
	}

	NativeFunction& func = AddUniqueNativeFunction(funcName);
	func.versionIndex.push_back(std::make_pair(version, json["NativeFuncIndex"].to_int()));

	if (func.decls.size() > 0)
	{
		// Some games (Deus Ex for example) only change the capitalization of argument names
		// Lowercase the strings and check those so we don't add another decl unnecessarily
		std::string funcArgsLower;
		funcArgsLower.resize(funcArgs.size());
		std::transform(funcArgs.begin(), funcArgs.end(), funcArgsLower.begin(), [](unsigned char c) { return tolower(c); });

		// Need to check all existing function declarations.
		// Games can have different versions of the same function.
		for (auto decl : func.decls)
		{
			std::string declArgsLower;
			declArgsLower.resize(decl.args.size());
			std::transform(decl.args.begin(), decl.args.end(), declArgsLower.begin(), [](unsigned char c) {return tolower(c); });

			if (funcArgsLower == declArgsLower)
			{
				decl.games.push_back(version);
				return;
			}
		}
	}

	// Add a new declaration
	NativeFunctionDecl decl;
	decl.args = funcArgs;
	decl.games.push_back(version);
	func.decls.push_back(std::move(decl));
}

NativeCppUpdater::NativeFunction& NativeCppUpdater::NativeClass::AddUniqueNativeFunction(const std::string& funcName)
{
	for (int i = 0; i < funcs.size(); i++)
	{
		NativeFunction& func = funcs[i];
		if (funcName == func.name)
			return func;
	}

	NativeFunction func;
	func.name = funcName;
	funcs.push_back(std::move(func));
	return funcs.back();
}

NativeCppUpdater::NativeProperty& NativeCppUpdater::NativeClass::AddUniqueNativeProperty(const std::string& propName)
{
	for (int i = 0; i < props.size(); i++)
	{
		NativeProperty& prop = props[i];
		if (propName == prop.name)
			return prop;
	}

	NativeProperty prop;
	prop.name = propName;
	props.push_back(std::move(prop));
	return props.back();
}