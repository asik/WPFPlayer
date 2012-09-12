#include "stdafx.h"
#include "NativeApisInterop.h"
#include "Helper.h"
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
}

using namespace System;
using namespace System::Runtime::InteropServices;

namespace NativeApisInterop {

	void NativeApisInteropClass::Init(IntPtr handle, int width, int height) {
		m_width = width;
		m_height = height;
		m_windowHandle = (HWND)handle.ToPointer();
		if (m_windowHandle) {
			m_deviceContext = GetDC(m_windowHandle);
			if (!m_deviceContext) {
				Helper::ErrorExit(L"BuildWindowCore");
			}

			uint PixelFormat;
			BYTE iAlphaBits = 0;
			BYTE iColorBits = 32;
			BYTE iDepthBits = 16;
			BYTE iAccumBits = 0;
			BYTE iStencilBits = 0;

			static PIXELFORMATDESCRIPTOR pfd =  {
				sizeof(PIXELFORMATDESCRIPTOR),	//size
				1,								//version
				PFD_DRAW_TO_WINDOW |				//flags
				PFD_SUPPORT_OPENGL |
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

			PixelFormat = ChoosePixelFormat(m_deviceContext, &pfd);
			if(!PixelFormat) {
				Helper::ErrorExit(L"BuildWindowCore");
			}

			if(!SetPixelFormat(m_deviceContext, PixelFormat, &pfd)) {
				Helper::ErrorExit(L"BuildWindowCore");
			}

			m_glContext = wglCreateContext(m_deviceContext);
			if(!m_glContext) {
				Helper::ErrorExit(L"BuildWindowCore");
			}

			if(!wglMakeCurrent(m_deviceContext, m_glContext))
			{
				Helper::ErrorExit(L"BuildWindowCore");
			}
			glViewport( 0, 0, width, height );
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_TEXTURE_2D);
		}

	}


	void NativeApisInteropClass::StartPlayback(String^ filePath) {

		char* filePathCString = (char*)(void*)Marshal::StringToHGlobalAnsi(filePath);
		av_register_all();

		AVFormatContext* formatContext;
		if (av_open_input_file(&formatContext, filePathCString, nullptr, 0, nullptr) != 0) {
			throw gcnew Exception("Could not find stream information");
		}

		int videoStream = -1;
		int audioStream = -1;
		for (int i=0; i < formatContext->nb_streams; i++) {
			AVMediaType codec_type = formatContext->streams[i]->codec->codec_type;
			if (videoStream < 0 && codec_type == CODEC_TYPE_VIDEO) {
				videoStream = i;
			}
			if (audioStream < 0 && codec_type == CODEC_TYPE_AUDIO) {
				audioStream = i;
			}
			if (audioStream > 0 && videoStream > 0) {
				break;
			}
		}

		if (videoStream == -1) {		
			throw gcnew Exception("Didn't find a video stream");
		}
		if (audioStream == -1) {		
			throw gcnew Exception("Didn't find an audio stream");
		}

		AVCodecContext* videoCodecCtx = formatContext->streams[videoStream]->codec;
		AVCodecContext* audioCodecCtx = formatContext->streams[audioStream]->codec;

		// Find the decoder for the video stream
		AVCodec *pCodec = avcodec_find_decoder(videoCodecCtx->codec_id);
		if (!pCodec) {
			throw gcnew Exception("Unsupported codec");
		}
		// Open codec
		if (avcodec_open(videoCodecCtx, pCodec) < 0) {
			throw gcnew Exception("Could not open codec");
		}

		// Allocate video frame
		AVFrame *pFrame = avcodec_alloc_frame();
		if (!pFrame) {
			throw gcnew Exception("Could not allocate a video frame");
		}

		// Allocate an AVFrame structure
		AVFrame *pFrameRGB = avcodec_alloc_frame();
		if (!pFrameRGB) {
			throw gcnew Exception("Could not allocate an AVFrame structure");
		}

		// Determine required buffer size and allocate buffer
		int numBytes = avpicture_get_size(PIX_FMT_RGB24, videoCodecCtx->width, videoCodecCtx->height);
		uint8_t *buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

		// Assign appropriate parts of buffer to image planes in pFrameRGB
		// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
		// of AVPicture
		avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, videoCodecCtx->width, videoCodecCtx->height);

		int frameFinished;
		AVPacket packet;

		int i = 0;
		while(av_read_frame(formatContext, &packet) >= 0) {
			// Is this a packet from the video stream?
			if(packet.stream_index==videoStream) {
				// Decode video frame
				avcodec_decode_video(videoCodecCtx, pFrame, &frameFinished,
					packet.data, packet.size);

				// Did we get a video frame?
				if(frameFinished) {
					int w = videoCodecCtx->width;
					int h = videoCodecCtx->height;
					SwsContext *img_convert_ctx = sws_getContext(
						w, 
						h, 
						videoCodecCtx->pix_fmt, 
						w, 
						h, 
						PIX_FMT_RGB24, 
						SWS_BICUBIC,
						NULL, 
						NULL, 
						NULL);
					sws_scale(
						img_convert_ctx, 
						pFrame->data, 
						pFrame->linesize, 
						0, 
						videoCodecCtx->height, 
						pFrameRGB->data,
						pFrameRGB->linesize);

					if(m_deviceContext == NULL || m_glContext == NULL)
						return;

					wglMakeCurrent(m_deviceContext, m_glContext);

					glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					glLoadIdentity();

					glTranslatef (0.0f, 0.0f, -2.6f);

					//glRotatef(m_fRotationAngle, 0.0f, 1.0f, 0.0f);

					glBegin (GL_TRIANGLES);
					glColor3f (1.0f, 0.0f, 0.0f);	glVertex3f(-1.0f,-1.0f, 0.0f);
					glColor3f (0.0f, 1.0f, 0.0f);	glVertex3f( 0.0f, 1.0f, 0.0f);
					glColor3f (0.0f, 0.0f, 1.0f);	glVertex3f( 1.0f,-1.0f, 0.0f);
					glEnd ();

					glFlush ();
					SwapBuffers(m_deviceContext);

					//glBegin (GL_TRIANGLES);
					//glColor3f (1.0f, 0.0f, 0.0f);	glVertex3f(-1.0f,-1.0f, 0.0f);
					//glColor3f (0.0f, 1.0f, 0.0f);	glVertex3f( 0.0f, 1.0f, 0.0f);
					//glColor3f (0.0f, 0.0f, 1.0f);	glVertex3f( 1.0f,-1.0f, 0.0f);
					//glEnd ();

					//glMatrixMode (GL_MODELVIEW);
					//glPushMatrix ();
					//glLoadIdentity ();
					//glMatrixMode (GL_PROJECTION);
					//glPushMatrix ();
					//glLoadIdentity ();
					//glBegin(GL_QUADS);
					//glBegin (GL_QUADS);
					//glVertex3i (-1, -1, -1);
					//glVertex3i (1, -1, -1);
					//glVertex3i (1, 1, -1);
					//glVertex3i (-1, 1, -1);
					//glEnd ();
					//glFlush ();
					//SwapBuffers(m_deviceContext);

					//SDL_DisplayYUVOverlay(bmp, &rect);
				}
			}
			//else if(packet.stream_index==audioStream) {
			//	packetQueue.Put(packet);
			//}
			else {
				// Free the packet that was allocated by av_read_frame
				av_free_packet(&packet);
			}
		}
	}

}