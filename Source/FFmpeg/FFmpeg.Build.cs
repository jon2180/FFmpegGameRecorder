// Copyright 2017-2024 MetaApp, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

struct ThirdPartyLibrary
{
	public bool IsValid;
	public string IncludeDirectory;
	public string LibraryDirectory;
}

public class FFmpeg : ModuleRules
{
	/// <summary>
	/// IsForceUseStaticLibrary indicate should use static library instead of shared library. Only for macOS
	/// </summary>
	public bool IsForceUseStaticLibrary = true;

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(PluginDirectory, "ThirdParty")); }
	}

	public FFmpeg(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		LoadFFmpeg();
	}

	void LoadFFmpeg()
	{
		// 定义宏
		ThirdPartyLibrary FfmpegLibrary;
		ThirdPartyLibrary X264Library;

		FfmpegLibrary.IncludeDirectory = Path.Combine(ThirdPartyPath, "ffmpeg", "include");
		X264Library.IncludeDirectory = Path.Combine(ThirdPartyPath, "ffmpeg", "include");

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		if ((Target.Platform == UnrealTargetPlatform.Win64))
		{
			string FfmpegLibraryPath = Path.Combine(ThirdPartyPath, "ffmpeg", "lib", "Win64dll");

			// 导入 library
			PublicAdditionalLibraries.AddRange(
				new[]
				{
					Path.Combine(FfmpegLibraryPath, "libx264.lib"),
					Path.Combine(FfmpegLibraryPath, "avcodec.lib"),
					//Path.Combine(FfmpegLibraryPath, "avdevice.lib"),
					Path.Combine(FfmpegLibraryPath, "avfilter.lib"),
					Path.Combine(FfmpegLibraryPath, "avformat.lib"),
					Path.Combine(FfmpegLibraryPath, "avutil.lib"),
					Path.Combine(FfmpegLibraryPath, "swresample.lib"),
					Path.Combine(FfmpegLibraryPath, "swscale.lib"),
				});

			// 导入 x264
			X264Library.IsValid = true;
			X264Library.LibraryDirectory = FfmpegLibraryPath.Replace('\\', '/');
			string[] X264Dlls = { "libx264-164.dll" };
			foreach (string Dll in X264Dlls)
			{
				LoadDll(X264Library.LibraryDirectory, Dll);
			}


			// 导入 ffmpeg
			FfmpegLibrary.IsValid = true;
			FfmpegLibrary.LibraryDirectory = FfmpegLibraryPath.Replace('\\', '/');
			string[] FfmpegDlls =
			{
				"avcodec-60.dll", "avfilter-9.dll",
				"avformat-60.dll", "avutil-58.dll",
				"swresample-4.dll", "swscale-7.dll",
				/*"postproc-57.dll","avdevice-60.dll",*/
            };
			foreach (string Dll in FfmpegDlls)
			{
				LoadDll(FfmpegLibrary.LibraryDirectory, Dll);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (IsForceUseStaticLibrary)
			{
				string FfmpegLibDir = Path.Combine(ThirdPartyPath, "ffmpeg", "lib", "macOS");

				// x264
				X264Library.IsValid = true;
				X264Library.LibraryDirectory = FfmpegLibDir;
				PublicAdditionalLibraries.Add(Path.Combine(FfmpegLibDir, "libx264.a"));

				// 导入 ffmpeg
				FfmpegLibrary.IsValid = true;
				FfmpegLibrary.LibraryDirectory = FfmpegLibDir;
				string[] Libs =
				{
					"libavcodec.a", "libavfilter.a", "libavformat.a", "libavutil.a", "libswresample.a", "libswscale.a"
				};
				foreach (string Lib in Libs)
				{
					PublicAdditionalLibraries.Add(Path.Combine(FfmpegLibDir, Lib));
				}
				PublicSystemLibraries.AddRange(new[] { "z", "bz2", "iconv" });
				PublicFrameworks.Add("VideoToolbox");
			}
			else
			{
				// Warning: 暂时不能用
				string FfmpegLibDir = Path.Combine(ThirdPartyPath, "ffmpeg", "lib", "macOSdylib");

				// TODO: x264 在哪里?
				X264Library.IsValid = true;
				X264Library.LibraryDirectory = FfmpegLibDir;
				PublicAdditionalLibraries.Add(Path.Combine(FfmpegLibDir, "libx264.164.dylib"));

				// 导入 ffmpeg
				FfmpegLibrary.IsValid = true;
				FfmpegLibrary.LibraryDirectory = FfmpegLibDir;
				string[] Libs =
				{
					"libavcodec.58.dylib", "libavfilter.7.dylib", "libavformat.58.dylib",
					"libavutil.56.dylib", "libswresample.3.dylib", "libswscale.5.dylib"
					// 	"libavcodec.58.134.100.dylib", "libavfilter.7.110.100.dylib", "libavformat.58.76.100.dylib",
					// 	"libavutil.56.70.100.dylib", "libswresample.3.9.100.dylib", "libswscale.5.9.100.dylib"
				};
				foreach (string Lib in Libs)
				{
					PublicAdditionalLibraries.Add(Path.Combine(FfmpegLibDir, Lib));
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			FfmpegLibrary.IsValid = true;
			X264Library.IsValid = true;

			string FfmpegLibDir = Path.Combine(ThirdPartyPath, "ffmpeg", "lib", "Android");

			X264Library.LibraryDirectory = FfmpegLibDir;
			FfmpegLibrary.LibraryDirectory = FfmpegLibDir;

			PublicAdditionalLibraries.AddRange(new[]
			{
				// ------------------------ Arm
				// x264
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libx264.a"),
				// ffmpeg
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libavcodec.a"),
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libavfilter.a"),
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libavformat.a"),
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libavutil.a"),
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libswscale.a"),
				// Path.Combine(FfmpegLibDir,"armeabi-v7a",  "libavdevice.a"),
				Path.Combine(FfmpegLibDir, "armeabi-v7a", "libswresample.a"),
				// Path.Combine(FfmpegLibDir, "armeabi-v7a", "libpostproc.a"),

				// ------------------------ Arm64
				// x264
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libx264.a"),
				// ffmpeg
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libavcodec.a"),
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libavfilter.a"),
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libavformat.a"),
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libavutil.a"),
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libswscale.a"),
				// Path.Combine(FfmpegLibDir,"arm64-v8a",  "libavdevice.a"),
				Path.Combine(FfmpegLibDir, "arm64-v8a", "libswresample.a"),
				// Path.Combine(FfmpegLibDir, "arm64-v8a", "libpostproc.a"),
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			FfmpegLibrary.IsValid = true;
			X264Library.IsValid = true;

			string FfmpegLibDir = Path.Combine(ThirdPartyPath, "ffmpeg", "lib", "iOS");

			X264Library.LibraryDirectory = FfmpegLibDir;
			FfmpegLibrary.LibraryDirectory = FfmpegLibDir;

			PublicSystemLibraries.AddRange(new[] { "z", "bz2", "iconv" });
			PublicFrameworks.AddRange(new []{
				"VideoToolbox", "AudioToolBox", "Foundation"
			});

			PublicAdditionalLibraries.AddRange(new[]
			{
				// x264
				Path.Combine(FfmpegLibDir, "libx264.a"),
				// ffmpeg
				Path.Combine(FfmpegLibDir, "libavcodec.a"),
				Path.Combine(FfmpegLibDir, "libavfilter.a"),
				Path.Combine(FfmpegLibDir, "libavformat.a"),
				Path.Combine(FfmpegLibDir, "libavutil.a"),
				Path.Combine(FfmpegLibDir, "libswscale.a"),
				// Path.Combine(FfmpegLibDir, "libavdevice.a"),
				Path.Combine(FfmpegLibDir, "libswresample.a"),
				// Path.Combine(FfmpegLibDir, "libpostproc.a"),
			});
		}
		else /* if (Target.Platform == UnrealTargetPlatform.Linux) */
		{
			// 跳过
			X264Library.IsValid = false;
			X264Library.LibraryDirectory = "";
			FfmpegLibrary.IsValid = false;
			FfmpegLibrary.LibraryDirectory = "";
		}

		if (X264Library.IsValid)
		{
			PublicIncludePaths.Add(X264Library.IncludeDirectory);
		}
		if (FfmpegLibrary.IsValid)
		{
			PublicIncludePaths.Add(FfmpegLibrary.IncludeDirectory);
		}

		// 提供给库的动态加载时使用的路径，避免在 C++ 层重复判断运行环境
		PrivateDefinitions.Add(string.Format("HAVE_X264={0}", X264Library.IsValid ? "1" : "0"));
		PrivateDefinitions.Add(string.Format("X264_LIBRARY_PATH=\"{0}\"", X264Library.LibraryDirectory));

		PrivateDefinitions.Add(string.Format("HAVE_FFMPEG={0}", FfmpegLibrary.IsValid ? "1" : "0"));
		PrivateDefinitions.Add(string.Format("FFMPEG_LIBRARY_PATH=\"{0}\"", FfmpegLibrary.LibraryDirectory));
	}

	void LoadDll(string DllPath, string DllName)
	{
		PublicDelayLoadDLLs.Add(DllName);
		RuntimeDependencies.Add(Path.Combine(DllPath, DllName), StagedFileType.NonUFS);
	}
}