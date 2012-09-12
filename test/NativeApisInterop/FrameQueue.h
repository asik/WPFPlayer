#pragma once
#include <list>

extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
}
#include "ThreadSafeQueue.h"

struct FrameBuffer {
	AVFrame* Frame;
	uint8_t * Buffer;
	FrameBuffer() : Frame(nullptr), Buffer(nullptr) {}
};

ref class FrameQueue
{
	ThreadSafeQueue<FrameBuffer>^ m_unusedFrames;
	ThreadSafeQueue<FrameBuffer>^ m_frameList;
	EventWaitHandle^ m_waitHandle;
	int m_maxLength;
	AVFrame* m_rawFrame;
	int m_frameSize;
	int m_videoStream;
	int m_audioStream;
	int m_targetWidth;
	int m_targetHeight;
	bool m_noMoreFrames;
	AVCodecContext* m_videoCodecCtx;
	AVCodecContext* m_audioCodecCtx;
	AVFormatContext* m_formatContext;
	SwsContext* m_swsContext;

	FrameBuffer AllocateFrame();
	void DecodeFrame(FrameBuffer& rgbFrame);
public:
	FrameQueue(int maxLength);
	~FrameQueue();
	void Init(System::String^ videoFilePath, int width, int height);
	bool Update();
	FrameBuffer Take();
	void Recycle(FrameBuffer frame);
	bool NoMoreFrames() { return m_noMoreFrames; }
};

