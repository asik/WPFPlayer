#include "StdAfx.h"
#include "FrameQueue.h"
#include <algorithm>

using namespace System;
using namespace System::Runtime::InteropServices;


FrameQueue::FrameQueue(int maxLength)
	:m_maxLength(maxLength),
	m_unusedFrames(nullptr),
	m_frameList(nullptr),
	m_rawFrame(nullptr),
	m_frameSize(0),
	m_videoCodecCtx(nullptr),
	m_audioCodecCtx(nullptr),
	m_formatContext(nullptr),
	m_swsContext(nullptr) {
		m_frameList = gcnew ThreadSafeQueue<FrameBuffer>;
		m_unusedFrames = gcnew ThreadSafeQueue<FrameBuffer>;
		m_waitHandle = gcnew EventWaitHandle(false, EventResetMode::ManualReset);
}

FrameQueue::~FrameQueue() {
	FrameBuffer fb;
	while (m_frameList->TryTake(fb)) {
		av_free(fb.Frame);
		av_free(fb.Buffer);
	}
	while (m_unusedFrames->TryTake(fb)) {
		av_free(fb.Frame);
		av_free(fb.Buffer);
	}

	delete m_frameList;
	delete m_unusedFrames;
	av_free(m_rawFrame);
}

void FrameQueue::Init(String^ videoFilePath, int width, int height) {	
	m_noMoreFrames = false;
	m_targetWidth = width;
	m_targetHeight = height;
	char* filePathCString = (char*)(void*)Marshal::StringToHGlobalAnsi(videoFilePath);
	av_register_all();

	AVFormatContext* tempFormatContext = m_formatContext; 
	// We cannot pass the address of m_formatContext because
	// it is part of a managed class and could thus be moved around by the GC.
	// A stack-based temporary does not present such risks.
	if (av_open_input_file(&tempFormatContext, filePathCString, nullptr, 0, nullptr) != 0) {
		throw gcnew Exception("Could not find stream information");
	}
	m_formatContext = tempFormatContext;

	m_videoStream = -1;
	m_audioStream = -1;
	for (int i=0; i < m_formatContext->nb_streams; i++) {
		AVMediaType codec_type = m_formatContext->streams[i]->codec->codec_type;
		if (m_videoStream < 0 && codec_type == CODEC_TYPE_VIDEO) {
			m_videoStream = i;
		}
		if (m_audioStream < 0 && codec_type == CODEC_TYPE_AUDIO) {
			m_audioStream = i;
		}
		if (m_audioStream > 0 && m_videoStream > 0) {
			break;
		}
	}

	if (m_videoStream == -1) {		
		throw gcnew Exception("Didn't find a video stream");
	}
	if (m_audioStream == -1) {		
		throw gcnew Exception("Didn't find an audio stream");
	}

	m_videoCodecCtx = m_formatContext->streams[m_videoStream]->codec;
	m_audioCodecCtx = m_formatContext->streams[m_audioStream]->codec;

	// Find the decoder for the video stream
	AVCodec *pCodec = avcodec_find_decoder(m_videoCodecCtx->codec_id);
	if (!pCodec) {
		throw gcnew Exception("Unsupported codec");
	}
	// Open codec
	if (avcodec_open(m_videoCodecCtx, pCodec) < 0) {
		throw gcnew Exception("Could not open codec");
	}

	// Allocate video frame
	m_rawFrame = avcodec_alloc_frame();
	if (!m_rawFrame) {
		throw gcnew Exception("Could not allocate a video frame");
	}

	m_frameSize = avpicture_get_size(PIX_FMT_RGB24, m_targetWidth, m_targetHeight);
}

bool FrameQueue::Update() {

	// If queue is already full, exit
	if (m_frameList->Count() >= m_maxLength) {
		return false;
	}

	// Decode data into frame
	AVPacket packet;

	if (av_read_frame(m_formatContext, &packet) >= 0) {
		// Is this a packet from the video stream?
		if(packet.stream_index == m_videoStream) {
			// Decode video frame
			int frameFinished;
			avcodec_decode_video2(m_videoCodecCtx, m_rawFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if(frameFinished) {

				// If rbg frame is available from unused list take it, otherwise create a new one
				FrameBuffer rgbFrame;
				if (!m_unusedFrames->TryTake(rgbFrame)) {
					rgbFrame = AllocateFrame();
				}

				DecodeFrame(rgbFrame);
				// Add frame to queue and signal so that consumer thread is unblocked if it was blocked
				m_frameList->Put(rgbFrame);
				m_waitHandle->Set();
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
	else {
		m_noMoreFrames = true;
	}
	return true;
}

FrameBuffer FrameQueue::Take() {
	FrameBuffer rgbFrame;
	while (!m_frameList->TryTake(rgbFrame)) {
		// If there are no more frames to produce, don't wait here forever
		if (m_noMoreFrames) {
			break;
		}
		// If there is no frame available, wait for the next one
		m_waitHandle->Reset();
		m_waitHandle->WaitOne();
	}
	return rgbFrame;
}


void FrameQueue::Recycle(FrameBuffer frame) {
	m_unusedFrames->Put(frame);
}

FrameBuffer FrameQueue::AllocateFrame() {
	FrameBuffer rgbFrame;
	rgbFrame.Frame = avcodec_alloc_frame();
	if (!rgbFrame.Frame) {
		throw gcnew Exception("Could not allocate a video frame");
	}
	rgbFrame.Buffer = (uint8_t *)av_malloc(m_frameSize*sizeof(uint8_t));
	avpicture_fill((AVPicture *)rgbFrame.Frame, rgbFrame.Buffer, PIX_FMT_RGB24, m_targetWidth, m_targetHeight);
	return rgbFrame;
}

void FrameQueue::DecodeFrame(FrameBuffer& rgbFrame) {

	if (!m_swsContext) {
		m_swsContext = sws_getContext(
			m_videoCodecCtx->width, 
			m_videoCodecCtx->height, 
			m_videoCodecCtx->pix_fmt, 
			m_targetWidth, 
			m_targetHeight, 
			PIX_FMT_RGB24, 
			SWS_BICUBIC,
			NULL, 
			NULL, 
			NULL);
	}
	sws_scale(
		m_swsContext, 
		m_rawFrame->data, 
		m_rawFrame->linesize, 
		0, 
		m_videoCodecCtx->height, 
		rgbFrame.Frame->data,
		rgbFrame.Frame->linesize);
}