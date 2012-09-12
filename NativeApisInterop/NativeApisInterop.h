#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "OpenGL.h"

namespace NativeApisInterop {

	public ref class NativeApisInteropClass
	{
	public:
		void Init(System::IntPtr handle, int width, int height);
		void StartPlayback(System::String^ filePath);
	private:
		HWND m_windowHandle;
		HDC m_deviceContext;
		HGLRC m_glContext;
		int m_width;
		int m_height;
	};
}
