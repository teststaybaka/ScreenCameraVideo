#include "Video.h"

void Video::initialStream() {
	avformat_alloc_output_context2(&oFormatCtx, NULL, NULL, outFile);
	if (!oFormatCtx) {
		qDebug("Memory error\n");
		exit(1);
	}
	ofmt = oFormatCtx->oformat;
	videoSt = NULL;
	audioSt = NULL;
	if (ofmt->video_codec != CODEC_ID_NONE) {
		oCodec = avcodec_find_encoder(ofmt->video_codec);
		if (!oCodec) {
			qDebug("oCodec not found\n");
			exit(1);
		}
		videoSt = avformat_new_stream(oFormatCtx, oCodec);
		if (!videoSt) {
			qDebug("Could not alloc stream\n");
			exit(1);
		}
		avcodec_get_context_defaults3(videoSt->codec, oCodec);
		videoSt->codec->codec_id = ofmt->video_codec;
		videoSt->codec->bit_rate = screenWidth*screenHeight*12;
		videoSt->codec->width = screenWidth;
		videoSt->codec->height = screenHeight;
		videoSt->codec->time_base.den = fps;
		videoSt->codec->time_base.num = 1;
		videoSt->codec->gop_size = 30;
		videoSt->codec->pix_fmt = PIX_FMT_YUV420P;
		videoSt->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	if (ofmt->audio_codec != CODEC_ID_NONE) {
		o2Codec = avcodec_find_encoder(ofmt->audio_codec);
		if (!o2Codec) {
			qDebug("o2Codec not found\n");
			exit(1);
		}
		audioSt = avformat_new_stream(oFormatCtx, o2Codec);
		if (!audioSt) {
			qDebug("Could not alloc stream\n");
			exit(1);
		}
		audioSt->id = 1;
		audioSt->codec->bit_rate = 320000;
		audioSt->codec->sample_rate = 44100;
		audioSt->codec->sample_fmt = AV_SAMPLE_FMT_S16;
		audioSt->codec->channels = 2;
		audioSt->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	if (videoSt) {
		if (avcodec_open2(videoSt->codec, oCodec, NULL) < 0) {
			qDebug("could not open video codec\n");
			exit(1);
		}
	}
	if (audioSt) {
		if (avcodec_open2(audioSt->codec, o2Codec, NULL) < 0) {
			qDebug("could not open audio codec\n");
			exit(1);
		}
	}

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&oFormatCtx->pb, outFile, AVIO_FLAG_WRITE) < 0) {
			qDebug("Could not open '%s'\n", outFile);
			exit(1);
		}
	}
	avformat_write_header(oFormatCtx, NULL);

	frameYUV = avcodec_alloc_frame();
	if(frameYUV==NULL) {
		qDebug()<<"alloc frame failed";
		exit(1);
	}
	// Determine required buffer size and allocate buffer
	avpicture_alloc((AVPicture*)frameYUV, PIX_FMT_YUV420P, screenWidth, screenHeight);
	frameYUV->pts = 0;

	frameAUD = avcodec_alloc_frame();
	frameAUD->nb_samples = audioSt->codec->frame_size;
	buffer_size = av_samples_get_buffer_size(NULL, audioSt->codec->channels,
												audioSt->codec->frame_size, 
												audioSt->codec->sample_fmt, 0);
}

void Video::audioStart() {
	format.setFrequency(44100);
	format.setChannels(2);
	format.setSampleSize(16);
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::SignedInt);

	deviceInfo = QAudioDeviceInfo::defaultInputDevice();
	if (!deviceInfo.isFormatSupported(format)) {
		qWarning()<<"default format not supported try to use nearest";
		format = deviceInfo.nearestFormat(format);
	}
	qDebug()<<"f:"<<deviceInfo.supportedFrequencies();
	qDebug()<<"sr:"<<deviceInfo.supportedSampleRates();
	qDebug()<<"sz:"<<deviceInfo.supportedSampleSizes();
	qDebug()<<"st:"<<deviceInfo.supportedSampleTypes();
	qDebug()<<"bo:"<<deviceInfo.supportedByteOrders();
	audioInput = new QAudioInput(deviceInfo, format);
	audioInput->setBufferSize(buffer_size);
	device = audioInput->start();
	actual_buffer_size = audioInput->bufferSize();
	if (buffer_size < actual_buffer_size)
		samples = (uint8_t*)av_malloc(actual_buffer_size);
	else
		samples = (uint8_t*)av_malloc(buffer_size);
}

void Video::writeOneVideoFrame() {
	av_init_packet(&pktV);
	pktV.data = NULL;
	pktV.size = 0;
	sws_scale(img_convert_ctx,
				(const uint8_t*  const*)data,
				linesize,
				0,
				screenHeight,
				frameYUV->data,
				frameYUV->linesize);

	ret = avcodec_encode_video2(videoSt->codec, &pktV, frameYUV, &got_output);
	if (ret < 0) {
		qDebug("error encoding frame\n");
		exit(1);
	}
	pktV.stream_index = videoSt->index;
	/* If size is zero, it means the image was buffered. */
	if (got_output) {
		if (pktV.pts != AV_NOPTS_VALUE)
			pktV.pts = av_rescale_q(pktV.pts, videoSt->codec->time_base, videoSt->time_base);
		if (pktV.dts != AV_NOPTS_VALUE)
			pktV.dts = av_rescale_q(pktV.dts, videoSt->codec->time_base, videoSt->time_base);
		if (videoSt->codec->coded_frame->key_frame)
			pktV.flags |= AV_PKT_FLAG_KEY;

		/* Write the compressed frame to the media file. */
		ret = av_interleaved_write_frame(oFormatCtx, &pktV);
		av_free_packet(&pktV);
	} else {
		ret = 0;
	}
	if (ret != 0) {
		qDebug("Error while writing video frame\n");
		exit(1);
	}
	frameYUV->pts++;
}

void Video::writeOneAudioFrame() {
	qDebug("audio frame write");

	av_init_packet(&pktA);
	pktA.data = NULL;
	pktA.size = 0;
	qDebug()<<"buffer bytes available:"<<audioInput->bytesReady();
	qDebug()<<"actual buffer size:"<<audioInput->bufferSize();
	if (audioInput->bytesReady() <= 0)
		return;
	int read_size = device->read((char*)samples, actual_buffer_size);
	qDebug()<<"read_size"<<read_size;
	if (av_fifo_space(fifo) < read_size) {
		av_fifo_realloc2(fifo, av_fifo_size(fifo)+read_size);
	}
	av_fifo_generic_write(fifo, samples, read_size, 0);
	if (av_fifo_size(fifo) < buffer_size)
		return;

	av_fifo_generic_read(fifo, samples, buffer_size, 0);
	ret = avcodec_fill_audio_frame(frameAUD, audioSt->codec->channels, audioSt->codec->sample_fmt, samples, buffer_size, 0);
	if (ret < 0) {
		qDebug()<<"could not setup audio frame\n";
		exit(1);
	}
	ret = avcodec_encode_audio2(audioSt->codec, &pktA, frameAUD, &got_packet);
	if (ret < 0) {
		qDebug()<<"error encoding audio frame\n";
		exit(1);
	}
	pktA.stream_index = audioSt->index;
	if (got_packet) {
		if (pktA.pts != AV_NOPTS_VALUE)
			pktA.pts = av_rescale_q(pktA.pts, audioSt->codec->time_base, audioSt->time_base);
		if (pktA.dts != AV_NOPTS_VALUE)
			pktA.dts = av_rescale_q(pktA.dts, audioSt->codec->time_base, audioSt->time_base);
		if (videoSt->codec->coded_frame->key_frame)
			pktA.flags |= AV_PKT_FLAG_KEY;

		if (av_interleaved_write_frame(oFormatCtx, &pktA) != 0) {
			qDebug()<<"Error while writing audio frame\n";
			exit(1);
		}
		av_free_packet(&pktA);
	}
}

void Video::audioStop() {
	audioInput->stop();
	delete audioInput;
}

void Video::writeVideoEnding() {
	got_output = 1;
	while(got_output) {
		av_init_packet(&pktV);
		pktV.data = NULL;
		pktV.size = 0;
		ret = avcodec_encode_video2(videoSt->codec, &pktV, NULL, &got_output);
		if (ret < 0) {
			qDebug("error encoding frame\n");
			exit(1);
		}
		pktV.stream_index = videoSt->index;
		if (got_output) {
				/* Write the compressed frame to the media file. */
				ret = av_interleaved_write_frame(oFormatCtx, &pktV);
				av_free_packet(&pktV);
		} else {
			ret = 0;
		}
		if (ret != 0) {
			qDebug("Error while writing video frame\n");
			exit(1);
		}
	}
}

void Video::writeAudioEnding() {
	while(av_fifo_size(fifo) > 0) {
		av_init_packet(&pktA);
		pktA.data = NULL;
		pktA.size = 0;

		av_fifo_generic_read(fifo, samples, buffer_size, 0);
		ret = avcodec_fill_audio_frame(frameAUD, audioSt->codec->channels,
									audioSt->codec->sample_fmt,
									samples, buffer_size, 0);
		if (ret < 0) {
			qDebug("could not setup audio frame\n");
			exit(1);
		}

		ret = avcodec_encode_audio2(audioSt->codec, &pktA, frameAUD, &got_packet);
		if (ret < 0) {
			qDebug("error encoding audio frame\n");
			exit(1);
		}
		pktA.stream_index = audioSt->index;
		if (got_packet) {
			if (av_interleaved_write_frame(oFormatCtx, &pktA) != 0) {
				qDebug("Error while writing audio frame\n");
				exit(1);
			}
			av_free_packet(&pktA);
		}
	}
}

void Video::releaseStream() {
	av_write_trailer(oFormatCtx);
	if (videoSt) {
		avcodec_close(videoSt->codec);
		av_free(frameYUV->data[0]);
		av_free(frameYUV);
	}
	if (audioSt) {
		avcodec_close(audioSt->codec);
		av_free(frameAUD->data[0]);
		av_free(frameAUD);
	}
	for (int i = 0; i < oFormatCtx->nb_streams; i++) {
        av_freep(&oFormatCtx->streams[i]->codec);
        av_freep(&oFormatCtx->streams[i]);
    }
	if (!(ofmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_close(oFormatCtx->pb);

	av_free(oFormatCtx);
}
