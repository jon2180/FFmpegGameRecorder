// Copyright 2017-2024 MetaApp, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Libyuv : ModuleRules
{
	/// <summary>
	/// IsForceUseStaticLibrary indicate should use static library instead of shared library. Only for macOS
	/// </summary>
	public bool IsForceUseStaticLibrary = true;

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(PluginDirectory, "ThirdParty")); }
	}

	public Libyuv(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string IncludeDirectory = Path.Combine(ThirdPartyPath, "libyuv", "include");
		string LibraryDirectory = "";
		bool IsValid = false;
		
		if ((Target.Platform == UnrealTargetPlatform.Win64))
		{
			string LibyuvLibraryPath = Path.Combine(ThirdPartyPath, "libyuv", "lib", "Win64dll");

			// 导入 library
			PublicAdditionalLibraries.AddRange(
				new[]
				{
					Path.Combine(LibyuvLibraryPath, "yuv.lib")
				});

			// 导入 yuv
			IsValid = true;
			LibraryDirectory = LibyuvLibraryPath.Replace('\\', '/');
			LoadDll(LibraryDirectory, "libyuv.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (IsForceUseStaticLibrary)
			{
				string LibyuvLibDir = Path.Combine(ThirdPartyPath, "libyuv", "lib", "macOS");

				// 导入 Yuv
				IsValid = true;
				LibraryDirectory = LibyuvLibDir;
				PublicAdditionalLibraries.Add(Path.Combine(LibyuvLibDir, "libyuv_internal.a"));
			}
			else
			{
				// Warning: 暂时不能用
				string LibyuvLibDir = Path.Combine(ThirdPartyPath, "libyuv", "lib", "macOSdylib");

				// 导入 Yuv
				IsValid = true;
				LibraryDirectory = LibyuvLibDir;
				PublicAdditionalLibraries.Add(Path.Combine(LibyuvLibDir, "libyuv.dylib"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			IsValid = true;

			string LibyuvLibDir = Path.Combine(ThirdPartyPath, "libyuv", "lib", "Android");

			LibraryDirectory = LibyuvLibDir;

			PublicAdditionalLibraries.AddRange(new[]
			{
				// ------------------------ Arm
				// libyuv
				Path.Combine(LibyuvLibDir, "armeabi-v7a", "libyuv.a"),

				// ------------------------ Arm64
				// libyuv
				Path.Combine(LibyuvLibDir, "arm64-v8a", "libyuv.a"),
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			IsValid = true;

			string LibyuvLibDir = Path.Combine(ThirdPartyPath, "libyuv", "lib", "iOS");

			LibraryDirectory = LibyuvLibDir;

			PublicAdditionalLibraries.AddRange(new[]
			{
				// libyuv
				Path.Combine(LibyuvLibDir, "libyuv_internal.a"),
				Path.Combine(LibyuvLibDir, "libyuv_neon.a"),
			});
		}
		else /* if (Target.Platform == UnrealTargetPlatform.Linux) */
		{
			// 跳过
			IsValid = false;
			LibraryDirectory = "";
		}

		if (IsValid)
		{
			PublicIncludePaths.Add(IncludeDirectory);
		}

		// 提供给库的动态加载时使用的路径，避免在 C++ 层重复判断运行环境
		PrivateDefinitions.Add(string.Format("HAVE_YUV={0}", IsValid ? "1" : "0"));
		PrivateDefinitions.Add(string.Format("YUV_LIBRARY_PATH=\"{0}\"", LibraryDirectory));
	}

	void LoadDll(string DllPath, string DllName)
	{
		PublicDelayLoadDLLs.Add(DllName);
		RuntimeDependencies.Add(Path.Combine(DllPath, DllName), StagedFileType.NonUFS);
	}
}