#pragma once

// Exclude rarely used parts of the windows headers
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Helper.h"
#include "FrameQueue.h"
#include "OpenGL.h"
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
}

// To use these, we must add some references...
//	o PresentationFramework (for HwndHost)
//		* PresentationCore
//		* WindowsBase
using namespace System;
using namespace System::Windows;
using namespace System::Windows::Threading;
using namespace System::Windows::Interop;
using namespace System::Windows::Input;
using namespace System::Windows::Media;
using namespace System::Windows::Forms; // We derive from UserControl this time, so this ref is necessary
using namespace System::ComponentModel;
using namespace System::Runtime::InteropServices;

namespace WPFOpenGLLib {

#pragma pack(1)
	struct RGB { unsigned char r; unsigned char g; unsigned char b; };

	public ref class OpenGLUserControl : public UserControl {
	private:
		HDC	m_hDC;
		HWND m_hWnd;
		HGLRC m_hRC;
		FrameQueue^ m_frameQueue;
		char* m_reverseRowBuffer;

		System::ComponentModel::Container^ components;
		DispatcherTimer^ m_dispatcherTimer;
		BackgroundWorker^ m_producerThread;
	public:

		OpenGLUserControl() : components(nullptr),
			m_hDC(nullptr),
			m_hWnd(nullptr),
			m_hRC(nullptr),
			m_reverseRowBuffer(nullptr) {
				InitializeComponent();
				this->components = gcnew System::ComponentModel::Container();

				//Attach load and size change event handlers
				this->Load += gcnew System::EventHandler(this, &OpenGLUserControl::InitializeOpenGL);
				this->SizeChanged += gcnew EventHandler(this, &OpenGLUserControl::ResizeOpenGL);
				m_frameQueue = gcnew FrameQueue(10);
		}

		virtual ~OpenGLUserControl() {
			if(NULL != m_hRC) {
				wglDeleteContext(m_hRC);
				m_hRC = NULL;
			}

			if(NULL != m_hWnd && NULL != m_hDC) {
				ReleaseDC(m_hWnd, m_hDC);
				m_hDC = NULL;
			}

			// Don't destroy the HWND... we didn't allocate it!

			if (components) {
				delete components;
			}
			if (m_reverseRowBuffer) {
				free(m_reverseRowBuffer);
			}
		}

		virtual void StartPlayback(String^ filePath)  {
			m_frameQueue->Init(filePath, Width, Height);

			// Start producer, e.g. a background thread that runs continuously to supply new frames
			m_producerThread = gcnew BackgroundWorker();
			m_producerThread->DoWork += gcnew DoWorkEventHandler(this, &OpenGLUserControl::UpdateFrameQueue);
			m_producerThread->RunWorkerCompleted += gcnew RunWorkerCompletedEventHandler(this, &OpenGLUserControl::NoMoreFrames);
			m_producerThread->RunWorkerAsync();

			// Start consumer, e.g. a timer that will call the display function 
			// at a regular interval on the UI thread
			m_dispatcherTimer = gcnew DispatcherTimer();
			m_dispatcherTimer->Tick += gcnew EventHandler(this, &OpenGLUserControl::DisplayNextFrame);
			m_dispatcherTimer->Interval = TimeSpan::FromMilliseconds(1);//1000.0 / 30.0);
			m_dispatcherTimer->Start();
		}

	protected:
		void InitializeComponent(){
			this->Name = "OpenGLUserControl";
		}

		virtual void OnPaintBackground( PaintEventArgs^ e ) override { 
		}

		virtual void OnPaint( System::Windows::Forms::PaintEventArgs^ e ) override {
			wglMakeCurrent(m_hDC, m_hRC);
			glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			SwapBuffers(m_hDC);
		}

		/// <summary>
		///	Event handler called when the form is loaded.  It retrieves the controls
		///	window handle and device context and creates the rendering context.
		/// </summary>
		virtual void InitializeOpenGL( Object^ sender, EventArgs^ e) {

			// Get the HWND from the base object
			m_hWnd	= (HWND) this->Handle.ToPointer();

			if(m_hWnd) {
				m_hDC = GetDC(m_hWnd);
				if(!m_hDC) {
					NativeApisInterop::Helper::ErrorExit(L"BuildWindowCore");
				}

				// Create PixelFormatDescriptor and choose pixel format
				uint PixelFormat;

				BYTE iAlphaBits = 0;
				BYTE iColorBits = 32;
				BYTE iDepthBits = 16;
				BYTE iAccumBits = 0;
				BYTE iStencilBits = 0;

				static PIXELFORMATDESCRIPTOR pfd =  {
					sizeof(PIXELFORMATDESCRIPTOR),	//size
					1,								//version
					PFD_DRAW_TO_WINDOW|				//flags
					PFD_SUPPORT_OPENGL|
					PFD_DOUBLEBUFFER,
					PFD_TYPE_RGBA,					//pixeltype
					iColorBits,
					0, 0, 0, 0, 0, 0,				//color bits ignored
					iAlphaBits,						
					0,								//alpha shift ignored
					iAccumBits,						//accum. buffer
					0, 0, 0, 0,						//accum bits ignored
					iDepthBits,						//depth buffer
					iStencilBits,					//stencil buffer
					0,								//aux buffer
					PFD_MAIN_PLANE,					//layer type
					0,								//reserved
					0, 0, 0							//masks ignored
				};

				PixelFormat = ChoosePixelFormat(m_hDC, &pfd);
				if(!PixelFormat) {
					NativeApisInterop::Helper::ErrorExit(L"BuildWindowCore");
				}

				if(!SetPixelFormat(m_hDC, PixelFormat, &pfd)) {
					NativeApisInterop::Helper::ErrorExit(L"BuildWindowCore");
				}

				m_hRC = wglCreateContext(m_hDC);
				if(!m_hRC) {
					NativeApisInterop::Helper::ErrorExit(L"BuildWindowCore");
				}

				if(!wglMakeCurrent(m_hDC, m_hRC)) {
					NativeApisInterop::Helper::ErrorExit(L"BuildWindowCore");
				}

				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glDisable(GL_DEPTH_TEST);
				glMatrixMode ( GL_PROJECTION );
				glLoadIdentity();
				glMatrixMode ( GL_MODELVIEW );
				glLoadIdentity();
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			}


		}

		/// <summary>
		/// Event handler called when the control is resized.
		/// </summary>
		void ResizeOpenGL(Object^ sender, EventArgs^ e) {
			UNREF(e);
			UNREF(sender);

			if(m_hDC == NULL || m_hRC == NULL)
				return;

			if(Width == 0 || Height == 0)
				return;

			wglMakeCurrent(m_hDC, m_hRC);
			glViewport( 0, 0, Width, Height );

			glMatrixMode ( GL_PROJECTION );
			glLoadIdentity();
			glMatrixMode ( GL_MODELVIEW );
			glLoadIdentity();
		}
	private:
		void DisplayNextFrame(Object^ sender, EventArgs^ e) {
			
			FrameBuffer newFrame = m_frameQueue->Take();
			// If we arrived at the end of the stream, stop displaying frames
			if (newFrame.Frame == nullptr && m_frameQueue->NoMoreFrames()) {
				m_dispatcherTimer->Stop();
				return;
			}

			if (!wglMakeCurrent(m_hDC, m_hRC)) {
				throw gcnew Exception("Could not acquire context");
			}
			glClear(GL_COLOR_BUFFER_BIT);

			//TODO: write a pixel shader to do this on the gpu rather than the cpu
			ReverseRows(newFrame.Frame->data[0]);
			glDrawPixels(Width, Height, GL_RGB, GL_UNSIGNED_BYTE, newFrame.Frame->data[0]);

			SwapBuffers(m_hDC);
			m_frameQueue->Recycle(newFrame);
		}

		void UpdateFrameQueue(Object^ sender, DoWorkEventArgs ^ e) {
			while (!m_frameQueue->NoMoreFrames()) {
				if (!m_frameQueue->Update()) {
					// This sleep makes the difference between keeping a core 100% busy and using only ~2% cpu.
					// We only sleep if the queue is already full anyway (denoted by Update() returning false).
					// A lot of useful debugging can be done just by playing with longer sleeps.
					Thread::Sleep(1);
				}
			}
		}

		void NoMoreFrames(Object^ sender, RunWorkerCompletedEventArgs^ e) {
			System::Windows::MessageBox::Show("No more frames!");
		}

		void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
			FILE *pFile;
			char szFilename[32];
			int  y;

			// Open file
			sprintf(szFilename, "frame%d.ppm", iFrame);
			pFile=fopen(szFilename, "wb");
			if(pFile==NULL)
				return;

			// Write header
			fprintf(pFile, "P6\n%d %d\n255\n", width, height);

			// Write pixel data
			fwrite(pFrame->data[0], 1, width*3*height, pFile);

			// Close file
			fclose(pFile);
		}

		void ReverseRows(uint8_t* buffer) {
			RGB* dataAsRgb = (RGB*)buffer;
			int bottomY = 0;
			int topY = Height - 1;
			if (m_reverseRowBuffer == nullptr) {
				m_reverseRowBuffer = (char*)malloc(Width * sizeof(RGB));
			}
			while( bottomY < (Height / 2) ) {
				RGB* bottomRow = dataAsRgb + (bottomY * Width);
				RGB* topRow = dataAsRgb + (topY * Width);

				memcpy(m_reverseRowBuffer, bottomRow, Width * sizeof(RGB));
				memcpy(bottomRow, topRow, Width * sizeof(RGB));
				memcpy(topRow, m_reverseRowBuffer, Width * sizeof(RGB));
				++bottomY;
				--topY;
			}
		}
	};
};
