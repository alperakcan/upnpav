/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 by Alper Akcan
    email                : alper.akcan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include <SDL.h>

#include "libavutil/avstring.h"
#include "libavformat/avformat.h"
#include "libavformat/rtsp.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"

#include "common.h"

#define __debugf(a...) do { } while (0);

typedef enum {
	FFMPEG_EVENT_TYPE_UNKOWN,
	FFMPEG_EVENT_TYPE_QUIT,
	FFMPEG_EVENT_TYPE_ALLOC,
	FFMPEG_EVENT_TYPE_REFRESH,
	FFMPEG_EVENT_TYPE_TYPES,
} ffmpeg_event_type_t;

typedef struct ffmpeg_event_s {
	list_t head;
	ffmpeg_event_type_t type;
	void *data;
} ffmpeg_event_t;

typedef struct ffmpeg_timer_s {
	list_t head;
	long long timeout;
	long long interval;
	int (*callback) (struct ffmpeg_timer_s *timer);
	void *data;
} ffmpeg_timer_t;

typedef struct ffmpeg_packet_queue_s {
	AVPacketList *first_pkt;
    	AVPacketList *last_pkt;
    	int nb_packets;
    	int size;
    	int abort_request;
    	pthread_mutex_t mutex;
    	pthread_cond_t cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 1
typedef struct VideoPicture {
	double pts;                                  ///<presentation time stamp for this picture
	SDL_Overlay *bmp;
	int width, height; /* source height & width */
	int allocated;
} VideoPicture;

#if !defined(DECLARE_ALIGNED)
#define DECLARE_ALIGNED(n,t,v)      t v __attribute__ ((aligned (n)))
#endif

typedef struct ffmpeg_stream_s {
	char *uri;

	int audio_stream;
	int video_stream;
	int subtitle_stream;
	int video_frame_width;
	int video_frame_height;
	enum PixelFormat video_pixel_format;
	int generatepts;
	int wanted_audio_stream;
	int wanted_video_stream;
	int audio_disable;
	int video_disable;
	int thread_count;
	int av_sync_type;
	int decoder_reorder_pts;
	int sws_flags;

	AVPacket flush_packet;
	AVInputFormat *iformat;
	AVFormatContext *ic;

	int64_t audio_callback_time;

	double audio_clock;
	double audio_diff_cum; /* used for AV difference average computation */
	double audio_diff_avg_coef;
	double audio_diff_threshold;
	int audio_diff_avg_count;
	AVStream *audio_st;
	int audio_hw_buf_size;
	PacketQueue audioq;
	DECLARE_ALIGNED(16,uint8_t,audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2]);
	unsigned int audio_buf_size; /* in bytes */
	int audio_buf_index; /* in bytes */
	AVPacket audio_pkt;
	uint8_t *audio_pkt_data;
	int audio_pkt_size;

	double frame_timer;
	double frame_last_pts;
	double frame_last_delay;
	double video_clock;                          ///<pts of last decoded frame / predicted pts of next decoded frame
	AVStream *video_st;
	PacketQueue videoq;
	double video_current_pts;                    ///<current displayed pts (different from video_clock if frame fifos are used)
	int64_t video_current_pts_time;              ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts
	pthread_mutex_t pictq_mutex;
	pthread_cond_t pictq_cond;
	VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
	int pictq_size;
	int pictq_rindex;
	int pictq_windex;
	pthread_t video_thread;

	int step;
	int paused;
	int width;
	int height;

	double external_clock; /* external clock base */
	int64_t external_clock_time;

	list_t event_list;
	pthread_mutex_t event_mutex;

	list_t timer_list;
	pthread_mutex_t timer_mutex;

	int loop_running;
	int loop_stopped;
	pthread_t loop_thread;
	pthread_cond_t loop_cond;
	pthread_mutex_t loop_mutex;

	int decode_running;
	int decode_stopped;
	pthread_t decode_thread;
	pthread_cond_t decode_cond;
	pthread_mutex_t decode_mutex;
} ffmpeg_stream_t;

#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_SUBTITLEQ_SIZE (5 * 16 * 1024)

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SDL_AUDIO_BUFFER_SIZE 1024

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

enum {
	AV_SYNC_AUDIO_MASTER, /* default choice */
	AV_SYNC_VIDEO_MASTER,
	AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

static int ffmpeg_event_add (ffmpeg_stream_t *ffmpeg_stream, ffmpeg_event_type_t type, void *data)
{
	ffmpeg_event_t *e;
	e = (ffmpeg_event_t *) malloc(sizeof(ffmpeg_event_t));
	if (e == NULL) {
		__debugf("malloc(sizeof(ffmpeg_event_t)) failed");
		return -1;
	}
	memset(e, 0, sizeof(ffmpeg_event_t));
	e->type = type;
	e->data = data;
	__debugf("adding event type 0x%08x", type);
	pthread_mutex_lock(&ffmpeg_stream->event_mutex);
	list_add(&e->head, &ffmpeg_stream->event_list);
	pthread_cond_signal(&ffmpeg_stream->loop_cond);
	pthread_mutex_unlock(&ffmpeg_stream->event_mutex);
	__debugf("added event type 0x%08x", type);
	return 0;
}

static int ffmpeg_timer_add (ffmpeg_stream_t *ffmpeg_stream, long long timeout, int (*callback) (ffmpeg_timer_t *), void *data)
{
	ffmpeg_timer_t *t;
	if (callback == NULL) {
		__debugf("callback is NULL");
		return -1;
	}
	t = (ffmpeg_timer_t *) malloc(sizeof(ffmpeg_timer_t));
	if (t == NULL) {
		__debugf("malloc(sizeof(ffmpeg_timer_t)) failed");
		return -1;
	}
	memset(t, 0, sizeof(ffmpeg_timer_t));
	t->timeout = timeout;
	t->interval = timeout;
	t->callback = callback;
	t->data = data;
	__debugf("adding timer %ld", timeout);
	pthread_mutex_lock(&ffmpeg_stream->timer_mutex);
	list_add(&t->head, &ffmpeg_stream->timer_list);
	pthread_cond_signal(&ffmpeg_stream->loop_cond);
	pthread_mutex_unlock(&ffmpeg_stream->timer_mutex);
	__debugf("added timer %ld", timeout);
	return 0;
}

static long long ffmpeg_calculate_timeout (ffmpeg_stream_t *ffmpeg_stream)
{
	long long timeout;
	ffmpeg_timer_t *ffmpeg_timer;
	timeout = -1;
	__debugf("calculating timeout");
	pthread_mutex_lock(&ffmpeg_stream->timer_mutex);
	list_for_each_entry(ffmpeg_timer, &ffmpeg_stream->timer_list, head) {
		if (timeout == -1) {
			timeout = ffmpeg_timer->timeout;
		} else {
			timeout = MIN(timeout, ffmpeg_timer->timeout);
		}
	}
	pthread_mutex_unlock(&ffmpeg_stream->timer_mutex);
	__debugf("calculated timeout is %lld", timeout);
	return timeout;
}

long long __gettimeofday (void)
{
	long long tsec;
	long long tusec;
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		return -1;
	}
	tsec = ((long long) tv.tv_sec) * 1000;
	tusec = ((long long) tv.tv_usec) / 1000;
	return tsec + tusec;
}

static int ffmpeg_cond_timedwait (pthread_cond_t *cond, pthread_mutex_t *mutex, long long timeout)
{
	int ret;
	long long tval;
	struct timespec tspec;
	if (timeout == -1) {
		ret = pthread_cond_wait(cond, mutex);
	} else {
		tval = __gettimeofday();
		tspec.tv_sec = (tval / 1000) + (timeout / 1000);
		tspec.tv_nsec = ((tval % 1000) + (timeout % 1000)) * 1000 * 1000;
		if (tspec.tv_nsec > 1000000000) {
			tspec.tv_sec += 1;
			tspec.tv_nsec -= 1000000000;
		}
again:		ret = pthread_cond_timedwait(cond, mutex, &tspec);
		switch (ret) {
			case EINTR:
				goto again;
				break;
			case ETIMEDOUT:
				ret = 1;
				break;
			case 0:
				break;
			default:
				ret = -1;
				break;
		}
	}
	return ret;
}

/* get the current audio output buffer size, in samples. With SDL, we
   cannot have a precise information */
static int audio_write_get_buf_size (ffmpeg_stream_t *ffmpeg_stream)
{
	return ffmpeg_stream->audio_buf_size - ffmpeg_stream->audio_buf_index;
}

/* get the current audio clock value */
static double get_audio_clock (ffmpeg_stream_t *ffmpeg_stream)
{
	double pts;
	int hw_buf_size, bytes_per_sec;
	pts = ffmpeg_stream->audio_clock;
	hw_buf_size = audio_write_get_buf_size(ffmpeg_stream);
	bytes_per_sec = 0;
	if (ffmpeg_stream->audio_st) {
		bytes_per_sec = ffmpeg_stream->audio_st->codec->sample_rate * 2 * ffmpeg_stream->audio_st->codec->channels;
	}
	if (bytes_per_sec)
		pts -= (double) hw_buf_size / bytes_per_sec;
	return pts;
}

/* get the current video clock value */
static double get_video_clock (ffmpeg_stream_t *ffmpeg_stream)
{
	double delta;
	if (ffmpeg_stream->paused) {
		delta = 0;
	} else {
		delta = (av_gettime() - ffmpeg_stream->video_current_pts_time) / 1000000.0;
	}
	return ffmpeg_stream->video_current_pts + delta;
}

/* get the current external clock value */
static double get_external_clock (ffmpeg_stream_t *ffmpeg_stream)
{
	int64_t ti;
	ti = av_gettime();
	return ffmpeg_stream->external_clock + ((ti - ffmpeg_stream->external_clock_time) * 1e-6);
}

/* get the current master clock value */
static double get_master_clock (ffmpeg_stream_t *ffmpeg_stream)
{
	double val;

	if (ffmpeg_stream->av_sync_type == AV_SYNC_VIDEO_MASTER) {
		if (ffmpeg_stream->video_st)
			val = get_video_clock(ffmpeg_stream);
		else
			val = get_audio_clock(ffmpeg_stream);
	} else if (ffmpeg_stream->av_sync_type == AV_SYNC_AUDIO_MASTER) {
		if (ffmpeg_stream->audio_st)
			val = get_audio_clock(ffmpeg_stream);
		else
			val = get_video_clock(ffmpeg_stream);
	} else {
		val = get_external_clock(ffmpeg_stream);
	}
	return val;
}

SDL_Surface *screen;

static int refresh_timer_callback (ffmpeg_timer_t *ffmpeg_timer)
{
	ffmpeg_stream_t *ffmpeg_stream;
	ffmpeg_stream = (ffmpeg_stream_t *) ffmpeg_timer->data;
	ffmpeg_event_add(ffmpeg_stream, FFMPEG_EVENT_TYPE_REFRESH, ffmpeg_stream);
	return 1;
}

/* schedule a video refresh in 'delay' ms */
static void schedule_refresh (ffmpeg_stream_t *ffmpeg_stream, long long delay)
{
	__debugf("adding timer %d", delay);
	ffmpeg_timer_add(ffmpeg_stream, delay, refresh_timer_callback, ffmpeg_stream);
}

static void alloc_picture (ffmpeg_stream_t *ffmpeg_stream)
{
	VideoPicture *vp;

	vp = &ffmpeg_stream->pictq[ffmpeg_stream->pictq_windex];

	if (vp->bmp)
		SDL_FreeYUVOverlay(vp->bmp);

	vp->bmp = SDL_CreateYUVOverlay(ffmpeg_stream->video_st->codec->width,
			ffmpeg_stream->video_st->codec->height,
			SDL_YV12_OVERLAY,
			screen);
	vp->width = ffmpeg_stream->video_st->codec->width;
	vp->height = ffmpeg_stream->video_st->codec->height;

	pthread_mutex_lock(&ffmpeg_stream->pictq_mutex);
	vp->allocated = 1;
	pthread_cond_signal(&ffmpeg_stream->pictq_cond);
	pthread_mutex_unlock(&ffmpeg_stream->pictq_mutex);
}

static int video_open (ffmpeg_stream_t *ffmpeg_stream)
{
	int w;
	int h;

	if (ffmpeg_stream->video_st &&
	    ffmpeg_stream->video_st->codec->width &&
	    ffmpeg_stream->video_st->codec->height) {
		w = ffmpeg_stream->video_st->codec->width;
		h = ffmpeg_stream->video_st->codec->height;
		debugf("w: %d, h: %d", w, h);
		screen = SDL_SetVideoMode(w, h, 0, SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL);
		if (!screen) {
			__debugf("SDL: could not set video mode - exiting");
			return -1;
		}
		SDL_WM_SetCaption("FFMpeg Render", "FFMpeg Render");
		ffmpeg_stream->width = screen->w;
		ffmpeg_stream->height = screen->h;
	} else {
		screen = SDL_SetVideoMode(0, 0, 0, 0);
	}
	return 0;
}

static void video_image_display (ffmpeg_stream_t *ffmpeg_stream)
{
	VideoPicture *vp;
	float aspect_ratio;
	int width, height, x, y;
	SDL_Rect rect;

	vp = &ffmpeg_stream->pictq[ffmpeg_stream->pictq_rindex];
	if (vp->bmp) {
		/* XXX: use variable in the frame */
		if (ffmpeg_stream->video_st->codec->sample_aspect_ratio.num == 0)
			aspect_ratio = 0;
		else
			aspect_ratio = av_q2d(ffmpeg_stream->video_st->codec->sample_aspect_ratio) * ffmpeg_stream->video_st->codec->width / ffmpeg_stream->video_st->codec->height;
		if (aspect_ratio <= 0.0)
			aspect_ratio = (float)ffmpeg_stream->video_st->codec->width / (float)ffmpeg_stream->video_st->codec->height;
		/* if an active format is indicated, then it overrides the
           	mpeg format */
		/* XXX: we suppose the screen has a 1.0 pixel ratio */
		height = ffmpeg_stream->height;
		width = ((int)rint(height * aspect_ratio)) & -3;
		if (width > ffmpeg_stream->width) {
			width = ffmpeg_stream->width;
			height = ((int)rint(width / aspect_ratio)) & -3;
		}
		x = (ffmpeg_stream->width - width) / 2;
		y = (ffmpeg_stream->height - height) / 2;
		rect.x = x;
		rect.y = y;
		rect.w = width;
		rect.h = height;
		SDL_DisplayYUVOverlay(vp->bmp, &rect);
	}
}

/* display the current picture, if any */
static void video_display (ffmpeg_stream_t *ffmpeg_stream)
{
	if(!screen)
		video_open(ffmpeg_stream);
//	if (ffmpeg_stream->audio_st && ffmpeg_stream->show_audio)
//		video_audio_display(ffmpeg_stream);
//	else
		if (ffmpeg_stream->video_st)
		video_image_display(ffmpeg_stream);
}

/* called to display each frame */
static void video_refresh (ffmpeg_stream_t *ffmpeg_stream)
{
	VideoPicture *vp;
	double actual_delay, delay, sync_threshold, ref_clock, diff;

	if (ffmpeg_stream->video_st) {
		if (ffmpeg_stream->pictq_size == 0) {
			/* if no picture, need to wait */
			schedule_refresh(ffmpeg_stream, 1);
		} else {
			/* dequeue the picture */
			vp = &ffmpeg_stream->pictq[ffmpeg_stream->pictq_rindex];

			/* update current video pts */
			ffmpeg_stream->video_current_pts = vp->pts;
			ffmpeg_stream->video_current_pts_time = av_gettime();

			/* compute nominal delay */
			delay = vp->pts - ffmpeg_stream->frame_last_pts;
			if (delay <= 0 || delay >= 2.0) {
				/* if incorrect delay, use previous one */
				delay = ffmpeg_stream->frame_last_delay;
			}
			ffmpeg_stream->frame_last_delay = delay;
			ffmpeg_stream->frame_last_pts = vp->pts;

			/* update delay to follow master synchronisation source */
			if (((ffmpeg_stream->av_sync_type == AV_SYNC_AUDIO_MASTER && ffmpeg_stream->audio_st) ||
					ffmpeg_stream->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
				/* if video is slave, we try to correct big delays by
                   	duplicating or deleting a frame */
				ref_clock = get_master_clock(ffmpeg_stream);
				diff = vp->pts - ref_clock;

				/* skip or repeat frame. We take into account the
                   	delay to compute the threshold. I still don't know
                   	if it is the best guess */
				sync_threshold = AV_SYNC_THRESHOLD;
				if (delay > sync_threshold)
					sync_threshold = delay;
				if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
					if (diff <= -sync_threshold)
						delay = 0;
					else if (diff >= sync_threshold)
						delay = 2 * delay;
				}
			}

			ffmpeg_stream->frame_timer += delay;
			/* compute the REAL delay (we need to do that to avoid
               	long term errors */
			actual_delay = ffmpeg_stream->frame_timer - (av_gettime() / 1000000.0);
			if (actual_delay < 0.010) {
				/* XXX: should skip picture */
				actual_delay = 0.010;
			}
			/* launch timer for next picture */
			schedule_refresh(ffmpeg_stream, (int)(actual_delay * 1000 + 0.5));

#if defined(DEBUG_SYNC)
			printf("video: delay=%0.3f actual_delay=%0.3f pts=%0.3f A-V=%f\n",
					delay, actual_delay, vp->pts, -diff);
#endif


			/* display picture */
			video_display(ffmpeg_stream);

			/* update queue size and signal for next picture */
			if (++ffmpeg_stream->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
				ffmpeg_stream->pictq_rindex = 0;

			pthread_mutex_lock(&ffmpeg_stream->pictq_mutex);
			ffmpeg_stream->pictq_size--;
			pthread_cond_signal(&ffmpeg_stream->pictq_cond);
			pthread_mutex_unlock(&ffmpeg_stream->pictq_mutex);
		}
	} else if (ffmpeg_stream->audio_st) {
		/* draw the next audio frame */

		schedule_refresh(ffmpeg_stream, 40);

		/* if only audio stream, then display the audio bars (better
           	than nothing, just to test the implementation */

		/* display picture */
		video_display(ffmpeg_stream);
	} else {
		schedule_refresh(ffmpeg_stream, 100);
	}
/*
	if (show_status) {
		static int64_t last_time;
		int64_t cur_time;
		int aqsize, vqsize, sqsize;
		double av_diff;

		cur_time = av_gettime();
		if (!last_time || (cur_time - last_time) >= 500 * 1000) {
			aqsize = 0;
			vqsize = 0;
			sqsize = 0;
			if (ffmpeg_stream->audio_st)
				aqsize = ffmpeg_stream->audioq.size;
			if (ffmpeg_stream->video_st)
				vqsize = ffmpeg_stream->videoq.size;
			if (ffmpeg_stream->subtitle_st)
				sqsize = ffmpeg_stream->subtitleq.size;
			av_diff = 0;
			if (ffmpeg_stream->audio_st && ffmpeg_stream->video_st)
				av_diff = get_audio_clock(is) - get_video_clock(is);
			printf("%7.2f A-V:%7.3f aq=%5dKB vq=%5dKB sq=%5dB    \r",
					get_master_clock(is), av_diff, aqsize / 1024, vqsize / 1024, sqsize);
			fflush(stdout);
			last_time = cur_time;
		}
	}
*/
}

static int ffmpeg_handle_event (ffmpeg_stream_t *ffmpeg_stream, long long timeout)
{
	int ret;
	ffmpeg_event_t *ffmpeg_event;
	ffmpeg_event_t *ffmpeg_event_next;
	ret = 0;
	pthread_mutex_lock(&ffmpeg_stream->event_mutex);
	if (list_count(&ffmpeg_stream->event_list) == 0) {
		ffmpeg_cond_timedwait(&ffmpeg_stream->loop_cond, &ffmpeg_stream->event_mutex, timeout);
	}
	list_for_each_entry_safe(ffmpeg_event, ffmpeg_event_next, &ffmpeg_stream->event_list, head) {
		list_del(&ffmpeg_event->head);
		switch (ffmpeg_event->type) {
			case FFMPEG_EVENT_TYPE_QUIT:
				__debugf("quit event received");
				ret = -1;
				break;
			case FFMPEG_EVENT_TYPE_ALLOC:
				video_open(ffmpeg_event->data);
				alloc_picture(ffmpeg_event->data);
				break;
			case FFMPEG_EVENT_TYPE_REFRESH:
				video_refresh(ffmpeg_event->data);
				break;
			default:
				__debugf("unkown event type");
				break;
		}
		free(ffmpeg_event);
	}
	pthread_mutex_unlock(&ffmpeg_stream->event_mutex);
	return ret;
}

static int ffmpeg_handle_timer (ffmpeg_stream_t *ffmpeg_stream, long long timedelta)
{
	ffmpeg_timer_t *ffmpeg_timer;
	ffmpeg_timer_t *ffmpeg_timer_next;
	pthread_mutex_lock(&ffmpeg_stream->timer_mutex);
	list_for_each_entry_safe(ffmpeg_timer, ffmpeg_timer_next, &ffmpeg_stream->timer_list, head) {
		ffmpeg_timer->interval -= timedelta;
		if (ffmpeg_timer->interval <= 0) {
			__debugf("executing timer");
			if (ffmpeg_timer->callback(ffmpeg_timer) == 0) {
				ffmpeg_timer->interval = ffmpeg_timer->timeout;
			} else {
				__debugf("deleting timer");
				list_del(&ffmpeg_timer->head);
				free(ffmpeg_timer);
			}
		}
	}
	pthread_mutex_unlock(&ffmpeg_stream->timer_mutex);
	return 0;
}

static void * ffmpeg_loop (void *arg)
{
	long long tv[2];
	long long timedel;
	long long timeout;
	ffmpeg_stream_t *ffmpeg_stream;

	ffmpeg_stream = (ffmpeg_stream_t *) arg;

	pthread_mutex_lock(&ffmpeg_stream->loop_mutex);
	__debugf("started loop");
	ffmpeg_stream->loop_running = 1;
	pthread_cond_signal(&ffmpeg_stream->loop_cond);
	pthread_mutex_unlock(&ffmpeg_stream->loop_mutex);

	do {
		timedel = 0;
		tv[0] = __gettimeofday();
		timeout = ffmpeg_calculate_timeout(ffmpeg_stream);
		if (ffmpeg_handle_event(ffmpeg_stream, timeout) != 0) {
			break;
		}
		tv[1] = __gettimeofday();
		timedel = tv[1] - tv[0];
		ffmpeg_handle_timer(ffmpeg_stream, timedel);
	} while (1);

	pthread_mutex_lock(&ffmpeg_stream->loop_mutex);
	ffmpeg_stream->loop_stopped = 1;
	pthread_cond_signal(&ffmpeg_stream->loop_cond);
	__debugf("stopped loop");
	pthread_mutex_unlock(&ffmpeg_stream->loop_mutex);

	return NULL;
}

static ffmpeg_stream_t *ffmpeg_stream_global;
static uint64_t global_video_pkt_pts= AV_NOPTS_VALUE;

static int ffmpeg_decode_interrupt_callback (void)
{
	int ret;
	ret = -1;
	if (ffmpeg_stream_global != NULL) {
		pthread_mutex_lock(&ffmpeg_stream_global->decode_mutex);
		ret = (ffmpeg_stream_global->decode_running == 1) ? 0 : -1;
		pthread_mutex_unlock(&ffmpeg_stream_global->decode_mutex);
	}
	return ret;
}

/* packet queue handling */
static void packet_queue_init (PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
}

static void packet_queue_flush (PacketQueue *q)
{
	AVPacketList *pkt;
	AVPacketList *pkt1;

	pthread_mutex_lock(&q->mutex);
	for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	pthread_mutex_unlock(&q->mutex);
}

static void packet_queue_end (PacketQueue *q)
{
	packet_queue_flush(q);
	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);
}

static int packet_queue_put (PacketQueue *q, AVPacket *pkt)
{
	AVPacketList *pkt1;

	/* duplicate the packet */
	if (av_dup_packet(pkt) < 0) {
		return -1;
	}

	pkt1 = av_malloc(sizeof(AVPacketList));
	if (!pkt1) {
		return -1;
	}
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	pthread_mutex_lock(&q->mutex);
	if (!q->last_pkt) {
		q->first_pkt = pkt1;
	} else {
		q->last_pkt->next = pkt1;
	}
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
	return 0;
}

static void packet_queue_abort (PacketQueue *q)
{
	pthread_mutex_lock(&q->mutex);
	q->abort_request = 1;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get (PacketQueue *q, AVPacket *pkt, int block)
{
	AVPacketList *pkt1;
	int ret;

	pthread_mutex_lock(&q->mutex);
	for(;;) {
		if (q->abort_request) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		} else if (!block) {
			ret = 0;
			break;
		} else {
			pthread_cond_wait(&q->cond, &q->mutex);
		}
	}
	pthread_mutex_unlock(&q->mutex);
	return ret;
}

/* return the new audio buffer size (samples can be added or deleted
   to get better sync if video or external master clock) */
static int synchronize_audio (ffmpeg_stream_t *ffmpeg_stream, short *samples, int samples_size1, double pts)
{
	int n, samples_size;
	double ref_clock;

	n = 2 * ffmpeg_stream->audio_st->codec->channels;
	samples_size = samples_size1;

	/* if not master, then we try to remove or add samples to correct the clock */
	if (((ffmpeg_stream->av_sync_type == AV_SYNC_VIDEO_MASTER && ffmpeg_stream->video_st) || ffmpeg_stream->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
		double diff, avg_diff;
		int wanted_size, min_size, max_size, nb_samples;
		ref_clock = get_master_clock(ffmpeg_stream);
		diff = get_audio_clock(ffmpeg_stream) - ref_clock;
		if (diff < AV_NOSYNC_THRESHOLD) {
			ffmpeg_stream->audio_diff_cum = diff + ffmpeg_stream->audio_diff_avg_coef * ffmpeg_stream->audio_diff_cum;
			if (ffmpeg_stream->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
				/* not enough measures to have a correct estimate */
				ffmpeg_stream->audio_diff_avg_count++;
			} else {
				/* estimate the A-V difference */
				avg_diff = ffmpeg_stream->audio_diff_cum * (1.0 - ffmpeg_stream->audio_diff_avg_coef);
				if (fabs(avg_diff) >= ffmpeg_stream->audio_diff_threshold) {
					wanted_size = samples_size + ((int)(diff * ffmpeg_stream->audio_st->codec->sample_rate) * n);
					nb_samples = samples_size / n;
					min_size = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX)) / 100) * n;
					max_size = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX)) / 100) * n;
					if (wanted_size < min_size)
						wanted_size = min_size;
					else if (wanted_size > max_size)
						wanted_size = max_size;

					/* add or remove samples to correction the synchro */
					if (wanted_size < samples_size) {
						/* remove samples */
						samples_size = wanted_size;
					} else if (wanted_size > samples_size) {
						uint8_t *samples_end, *q;
						int nb;

						/* add samples */
						nb = (samples_size - wanted_size);
						samples_end = (uint8_t *)samples + samples_size - n;
						q = samples_end + n;
						while (nb > 0) {
							memcpy(q, samples_end, n);
							q += n;
							nb -= n;
						}
						samples_size = wanted_size;
					}
				}
#if 0
				printf("diff=%f adiff=%f sample_diff=%d apts=%0.3f vpts=%0.3f %f\n",
						diff, avg_diff, samples_size - samples_size1,
						ffmpeg_stream->audio_clock, ffmpeg_stream->video_clock, ffmpeg_stream->audio_diff_threshold);
#endif
			}
		} else {
			/* too big difference : may be initial PTS errors, so
               		 * reset A-V filter
               		 */
			ffmpeg_stream->audio_diff_avg_count = 0;
			ffmpeg_stream->audio_diff_cum = 0;
		}
	}

	return samples_size;
}

/* decode one audio frame and returns its uncompressed size */
static int audio_decode_frame (ffmpeg_stream_t *ffmpeg_stream, uint8_t *audio_buf, int buf_size, double *pts_ptr)
{
	AVPacket *pkt = &ffmpeg_stream->audio_pkt;
	int n, len1, data_size;
	double pts;

	for(;;) {
		/* NOTE: the audio packet can contain several frames */
		while (ffmpeg_stream->audio_pkt_size > 0) {
			data_size = buf_size;
			len1 = avcodec_decode_audio2(ffmpeg_stream->audio_st->codec, (int16_t *) audio_buf, &data_size, ffmpeg_stream->audio_pkt_data, ffmpeg_stream->audio_pkt_size);
			if (len1 < 0) {
				/* if error, we skip the frame */
				ffmpeg_stream->audio_pkt_size = 0;
				break;
			}

			ffmpeg_stream->audio_pkt_data += len1;
			ffmpeg_stream->audio_pkt_size -= len1;
			if (data_size <= 0)
				continue;
			/* if no pts, then compute it */
			pts = ffmpeg_stream->audio_clock;
			*pts_ptr = pts;
			n = 2 * ffmpeg_stream->audio_st->codec->channels;
			ffmpeg_stream->audio_clock += (double) data_size / (double) (n * ffmpeg_stream->audio_st->codec->sample_rate);
#if defined(DEBUG_SYNC)
			{
				static double last_clock;
				printf("audio: delay=%0.3f clock=%0.3f pts=%0.3f\n",
						ffmpeg_stream->audio_clock - last_clock,
						ffmpeg_stream->audio_clock, pts);
				last_clock = ffmpeg_stream->audio_clock;
			}
#endif
			return data_size;
		}

		/* free the current packet */
		if (pkt->data)
			av_free_packet(pkt);

		if (ffmpeg_stream->paused || ffmpeg_stream->audioq.abort_request) {
			return -1;
		}

		/* read next packet */
		if (packet_queue_get(&ffmpeg_stream->audioq, pkt, 1) < 0)
			return -1;
		if (pkt->size == ffmpeg_stream->flush_packet.size &&
		    strncmp((char *) pkt->data, (char *) ffmpeg_stream->flush_packet.data, pkt->size) == 0) {
			avcodec_flush_buffers(ffmpeg_stream->audio_st->codec);
			continue;
		}

		ffmpeg_stream->audio_pkt_data = pkt->data;
		ffmpeg_stream->audio_pkt_size = pkt->size;

		/* if update the audio clock with the pts */
		if (pkt->pts != AV_NOPTS_VALUE) {
			ffmpeg_stream->audio_clock = av_q2d(ffmpeg_stream->audio_st->time_base) * pkt->pts;
		}
	}
}

/* prepare a new audio buffer */
static void sdl_audio_callback (void *opaque, Uint8 *stream, int len)
{
	ffmpeg_stream_t *ffmpeg_stream;
	int audio_size, len1;
	double pts;

	ffmpeg_stream = (ffmpeg_stream_t *) opaque;

	ffmpeg_stream->audio_callback_time = av_gettime();
	while (len > 0) {
		if (ffmpeg_stream->audio_buf_index >= ffmpeg_stream->audio_buf_size) {
			audio_size = audio_decode_frame(ffmpeg_stream, ffmpeg_stream->audio_buf, sizeof(ffmpeg_stream->audio_buf), &pts);
			if (audio_size < 0) {
				/* if error, just output silence */
				ffmpeg_stream->audio_buf_size = 1024;
				memset(ffmpeg_stream->audio_buf, 0, ffmpeg_stream->audio_buf_size);
			} else {
#if 0
				if (ffmpeg_stream->show_audio)
					update_sample_display(ffmpeg_stream, (int16_t *) ffmpeg_stream->audio_buf, audio_size);
#endif
				audio_size = synchronize_audio(ffmpeg_stream, (int16_t *) ffmpeg_stream->audio_buf, audio_size, pts);
				ffmpeg_stream->audio_buf_size = audio_size;
			}
			ffmpeg_stream->audio_buf_index = 0;
		}
		len1 = ffmpeg_stream->audio_buf_size - ffmpeg_stream->audio_buf_index;
		if (len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t *) ffmpeg_stream->audio_buf + ffmpeg_stream->audio_buf_index, len1);
		len -= len1;
		stream += len1;
		ffmpeg_stream->audio_buf_index += len1;
	}
}

static int my_get_buffer (struct AVCodecContext *c, AVFrame *pic)
{
	int ret= avcodec_default_get_buffer(c, pic);
	uint64_t *pts= av_malloc(sizeof(uint64_t));
	*pts= global_video_pkt_pts;
	pic->opaque= pts;
	return ret;
}

static void my_release_buffer (struct AVCodecContext *c, AVFrame *pic)
{
	if(pic) av_freep(&pic->opaque);
	avcodec_default_release_buffer(c, pic);
}

/**
 *
 * @param pts the dts of the pkt / pts of the frame and guessed if not known
 */
static int queue_picture (ffmpeg_stream_t *ffmpeg_stream, AVFrame *src_frame, double pts)
{
	VideoPicture *vp;
	int dst_pix_fmt;
	AVPicture pict;
	static struct SwsContext *img_convert_ctx;

	/* wait until we have space to put a new picture */
	pthread_mutex_lock(&ffmpeg_stream->pictq_mutex);
	while (ffmpeg_stream->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !ffmpeg_stream->videoq.abort_request) {
		pthread_cond_wait(&ffmpeg_stream->pictq_cond, &ffmpeg_stream->pictq_mutex);
	}
	pthread_mutex_unlock(&ffmpeg_stream->pictq_mutex);

	if (ffmpeg_stream->videoq.abort_request)
		return -1;

	vp = &ffmpeg_stream->pictq[ffmpeg_stream->pictq_windex];

	/* alloc or resize hardware picture buffer */
	if (!vp->bmp ||
	    vp->width != ffmpeg_stream->video_st->codec->width ||
	    vp->height != ffmpeg_stream->video_st->codec->height) {
		vp->allocated = 0;

		/* the allocation must be done in the main thread to avoid
           	   locking problems */
		ffmpeg_event_add(ffmpeg_stream, FFMPEG_EVENT_TYPE_ALLOC, ffmpeg_stream);

		/* wait until the picture is allocated */
		pthread_mutex_lock(&ffmpeg_stream->pictq_mutex);
		while (!vp->allocated && !ffmpeg_stream->videoq.abort_request) {
			pthread_cond_wait(&ffmpeg_stream->pictq_cond, &ffmpeg_stream->pictq_mutex);
		}
		pthread_mutex_unlock(&ffmpeg_stream->pictq_mutex);

		if (ffmpeg_stream->videoq.abort_request)
			return -1;
	}

	/* if the frame is not skipped, then display it */
	if (vp->bmp) {
		/* get a pointer on the bitmap */
		SDL_LockYUVOverlay (vp->bmp);

		dst_pix_fmt = PIX_FMT_YUV420P;
		pict.data[0] = vp->bmp->pixels[0];
		pict.data[1] = vp->bmp->pixels[2];
		pict.data[2] = vp->bmp->pixels[1];

		pict.linesize[0] = vp->bmp->pitches[0];
		pict.linesize[1] = vp->bmp->pitches[2];
		pict.linesize[2] = vp->bmp->pitches[1];
		img_convert_ctx = sws_getCachedContext(img_convert_ctx,
					ffmpeg_stream->video_st->codec->width,
					ffmpeg_stream->video_st->codec->height,
					ffmpeg_stream->video_st->codec->pix_fmt,
					ffmpeg_stream->video_st->codec->width,
					ffmpeg_stream->video_st->codec->height,
					dst_pix_fmt, ffmpeg_stream->sws_flags, NULL, NULL, NULL);
		if (img_convert_ctx == NULL) {
			fprintf(stderr, "Cannot initialize the conversion context\n");
			exit(1);
		}
		sws_scale(img_convert_ctx, src_frame->data, src_frame->linesize,
					0, ffmpeg_stream->video_st->codec->height, pict.data, pict.linesize);
		/* update the bitmap content */
		SDL_UnlockYUVOverlay(vp->bmp);

		vp->pts = pts;

		/* now we can update the picture count */
		if (++ffmpeg_stream->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
			ffmpeg_stream->pictq_windex = 0;
		pthread_mutex_lock(&ffmpeg_stream->pictq_mutex);
		ffmpeg_stream->pictq_size++;
		pthread_mutex_unlock(&ffmpeg_stream->pictq_mutex);
	}
	return 0;
}

/**
 * compute the exact PTS for the picture if it is omitted in the stream
 * @param pts1 the dts of the pkt / pts of the frame
 */
static int output_picture2 (ffmpeg_stream_t *ffmpeg_stream, AVFrame *src_frame, double pts1)
{
	double frame_delay, pts;

	pts = pts1;

	if (pts != 0) {
		/* update video clock with pts, if present */
		ffmpeg_stream->video_clock = pts;
	} else {
		pts = ffmpeg_stream->video_clock;
	}
	/* update video clock for next frame */
	frame_delay = av_q2d(ffmpeg_stream->video_st->codec->time_base);
	/* for MPEG2, the frame can be repeated, so we update the
       	   clock accordingly */
	frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
	ffmpeg_stream->video_clock += frame_delay;

	return queue_picture(ffmpeg_stream, src_frame, pts);
}

static void * video_thread (void *arg)
{
	ffmpeg_stream_t *ffmpeg_stream;
	AVPacket pkt1, *pkt = &pkt1;
	int len1, got_picture;
	AVFrame *frame= avcodec_alloc_frame();
	double pts;

	ffmpeg_stream = (ffmpeg_stream_t *) arg;

	for(;;) {
		while (ffmpeg_stream->paused && !ffmpeg_stream->videoq.abort_request) {
			SDL_Delay(10);
		}
		if (packet_queue_get(&ffmpeg_stream->videoq, pkt, 1) < 0)
			break;

		if (pkt->size == ffmpeg_stream->flush_packet.size &&
		    strncmp((char *) pkt->data, (char *) ffmpeg_stream->flush_packet.data, pkt->size) == 0) {
			avcodec_flush_buffers(ffmpeg_stream->video_st->codec);
			continue;
		}

		/* NOTE: ipts is the PTS of the _first_ picture beginning in
	           this packet, if any */
		global_video_pkt_pts= pkt->pts;
		len1 = avcodec_decode_video(ffmpeg_stream->video_st->codec, frame, &got_picture, pkt->data, pkt->size);

		if ((ffmpeg_stream->decoder_reorder_pts || pkt->dts == AV_NOPTS_VALUE) && frame->opaque && *(uint64_t *) frame->opaque != AV_NOPTS_VALUE)
			pts= *(uint64_t*)frame->opaque;
		else if(pkt->dts != AV_NOPTS_VALUE)
			pts= pkt->dts;
		else
			pts= 0;
		pts *= av_q2d(ffmpeg_stream->video_st->time_base);

		if (got_picture) {
			if (output_picture2(ffmpeg_stream, frame, pts) < 0)
				goto the_end;
		}
		av_free_packet(pkt);
//		if (ffmpeg_stream->step)
//			if (cur_stream)
//				stream_pause(cur_stream);
	}
the_end:
	av_free(frame);
	return NULL;
}

static void ffmpeg_stream_component_close (ffmpeg_stream_t *ffmpeg_stream, int stream_index)
{
	AVFormatContext *ic;
	AVCodecContext *enc;
	ic = ffmpeg_stream->ic;
	if (stream_index < 0 || stream_index >= ic->nb_streams) {
		return;
	}
	__debugf("closing component stream '%d'", stream_index);
	enc = ic->streams[stream_index]->codec;
	switch(enc->codec_type) {
		case CODEC_TYPE_AUDIO:
			packet_queue_abort(&ffmpeg_stream->audioq);
			SDL_CloseAudio();
			packet_queue_end(&ffmpeg_stream->audioq);
			break;
		case CODEC_TYPE_VIDEO:
			packet_queue_abort(&ffmpeg_stream->videoq);
			/* note: we also signal this mutex to make sure we deblock the
		           video thread in all cases */
			pthread_mutex_lock(&ffmpeg_stream->pictq_mutex);
			pthread_cond_signal(&ffmpeg_stream->pictq_cond);
			pthread_mutex_unlock(&ffmpeg_stream->pictq_mutex);
			pthread_join(ffmpeg_stream->video_thread, NULL);
			packet_queue_end(&ffmpeg_stream->videoq);
		        break;
		default:
			break;
	}
	if (avcodec_close(enc) < 0) {
		__debugf("avcodec_close() failed");
	}
	switch(enc->codec_type) {
		case CODEC_TYPE_AUDIO:
			ffmpeg_stream->audio_st = NULL;
			ffmpeg_stream->audio_stream = -1;
			break;
		case CODEC_TYPE_VIDEO:
			ffmpeg_stream->video_st = NULL;
			ffmpeg_stream->video_stream = -1;
			break;
		default:
			break;
	}
}

static int ffmpeg_stream_component_open (ffmpeg_stream_t *ffmpeg_stream, int stream_index)
{
	AVFormatContext *ic;
	AVCodecContext *enc;
	AVCodec *codec;
	SDL_AudioSpec wanted_spec, spec;

	ic = ffmpeg_stream->ic;
	if (stream_index < 0 || stream_index >= ic->nb_streams) {
		__debugf("invalid stream index");
		return -1;
	}
	enc = ic->streams[stream_index]->codec;

	if (enc->codec_type == CODEC_TYPE_AUDIO) {
		if (enc->channels > 0) {
			enc->request_channels = FFMIN(2, enc->channels);
		} else {
			enc->channels = 2;
		}
	}

	codec = avcodec_find_decoder(enc->codec_id);
	enc->debug_mv = 0;
	enc->debug = 0;
	enc->workaround_bugs = 0;
	enc->lowres = 0;
	if (enc->lowres) {
		enc->flags |= CODEC_FLAG_EMU_EDGE;
	}
	enc->idct_algo = FF_IDCT_AUTO;
	enc->skip_frame = AVDISCARD_DEFAULT;
	enc->skip_idct = AVDISCARD_DEFAULT;
	enc->skip_loop_filter = AVDISCARD_DEFAULT;
	enc->error_recognition = FF_ER_CAREFUL;
	enc->error_concealment = 3;
	if (!codec || avcodec_open(enc, codec) < 0) {
	        return -1;
	}
	/* prepare audio output */
	if (enc->codec_type == CODEC_TYPE_AUDIO) {
		wanted_spec.freq = enc->sample_rate;
		wanted_spec.format = AUDIO_S16SYS;
		wanted_spec.channels = enc->channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
		wanted_spec.callback = sdl_audio_callback;
		wanted_spec.userdata = ffmpeg_stream;
		if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
			__debugf("SDL_OpenAudio: %s", SDL_GetError());
			return -1;
		}
		ffmpeg_stream->audio_hw_buf_size = spec.size;
	}
	if (ffmpeg_stream->thread_count > 1) {
		avcodec_thread_init(enc, ffmpeg_stream->thread_count);
	}
	enc->thread_count = ffmpeg_stream->thread_count;
	switch (enc->codec_type) {
		case CODEC_TYPE_AUDIO:
			ffmpeg_stream->audio_stream = stream_index;
			ffmpeg_stream->audio_st = ic->streams[stream_index];
			ffmpeg_stream->audio_buf_size = 0;
			ffmpeg_stream->audio_buf_index = 0;
		        /* init averaging filter */
			ffmpeg_stream->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
			ffmpeg_stream->audio_diff_avg_count = 0;
			/* since we do not have a precise anough audio fifo fullness,
		           we correct audio sync only if larger than this threshold */
			ffmpeg_stream->audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / enc->sample_rate;

			memset(&ffmpeg_stream->audio_pkt, 0, sizeof(ffmpeg_stream->audio_pkt));
			packet_queue_init(&ffmpeg_stream->audioq);
			SDL_PauseAudio(0);
			break;
		case CODEC_TYPE_VIDEO:
			ffmpeg_stream->video_stream = stream_index;
			ffmpeg_stream->video_st = ic->streams[stream_index];

			ffmpeg_stream->frame_last_delay = 40e-3;
			ffmpeg_stream->frame_timer = (double) av_gettime() / 1000000.0;
			ffmpeg_stream->video_current_pts_time = av_gettime();

			packet_queue_init(&ffmpeg_stream->videoq);
			pthread_create(&ffmpeg_stream->video_thread, NULL, video_thread, ffmpeg_stream);
			enc->get_buffer = my_get_buffer;
			enc->release_buffer = my_release_buffer;
			break;
		default:
			break;
	}

	return 0;
}

static void * ffmpeg_decode (void *arg)
{
	ffmpeg_stream_t *ffmpeg_stream;

	int i;
	int error;
	int audio_index;
	int video_index;
	AVFormatContext *ic;
	AVPacket pkt1, *pkt = &pkt1;
	AVFormatParameters params, *ap = &params;

	ffmpeg_stream = (ffmpeg_stream_t *) arg;

	pthread_mutex_lock(&ffmpeg_stream->decode_mutex);
	ffmpeg_stream->decode_running = 1;
	ffmpeg_stream_global = ffmpeg_stream;
	url_set_interrupt_cb(ffmpeg_decode_interrupt_callback);
	pthread_cond_signal(&ffmpeg_stream->decode_cond);
	pthread_mutex_unlock(&ffmpeg_stream->decode_mutex);

	memset(ap, 0, sizeof(AVFormatParameters));
	ap->width = ffmpeg_stream->video_frame_width;
	ap->height = ffmpeg_stream->video_frame_height;
	ap->time_base = (AVRational) {1, 25};
	ap->pix_fmt = ffmpeg_stream->video_pixel_format;

	error = av_open_input_file(&ic, ffmpeg_stream->uri, ffmpeg_stream->iformat, 0, ap);
	if (error < 0) {
		__debugf("av_open_input_file(%s) failed", ffmpeg_stream->uri);
		goto out;
	}

	ffmpeg_stream->ic = ic;
	if (ffmpeg_stream->generatepts == 1) {
		ic->flags |= AVFMT_FLAG_GENPTS;
	}

	error = av_find_stream_info(ic);
	if (error < 0) {
		__debugf("av_find_stream_info() failed");
		goto out;
	}

	audio_index = -1;
	video_index = -1;
	for (i = 0; i < ic->nb_streams; i++) {
		AVCodecContext *cc;
		cc = ic->streams[i]->codec;
		switch (cc->codec_type) {
			case CODEC_TYPE_AUDIO:
				if ((audio_index < 0 || ffmpeg_stream->wanted_audio_stream-- > 0) && ffmpeg_stream->audio_disable == 0) {
					audio_index = i;
				}
				break;
			case CODEC_TYPE_VIDEO:
				if ((video_index < 0 || ffmpeg_stream->wanted_video_stream-- > 0) && ffmpeg_stream->video_disable == 0) {
					video_index = i;
				}
				break;
			default:
				__debugf("unkown codec type 0x%08x", cc->codec_type);
				break;
		}
	}
	__debugf("selected video (%d), audio (%d)", video_index, audio_index);

	if (audio_index >= 0) {
		ffmpeg_stream_component_open(ffmpeg_stream, audio_index);
	}
	if (video_index >= 0) {
		ffmpeg_stream_component_open(ffmpeg_stream, video_index);
	}
	if (ffmpeg_stream->audio_stream < 0 && ffmpeg_stream->video_stream < 0) {
		__debugf("could not open codecs");
		goto out;
	}

	do {
		pthread_mutex_lock(&ffmpeg_stream->decode_mutex);
		if (ffmpeg_stream->decode_running == 0) {
			pthread_mutex_unlock(&ffmpeg_stream->decode_mutex);
			break;
		}
		pthread_mutex_unlock(&ffmpeg_stream->decode_mutex);

		/* if the queue are full, no need to read more */
	        if (ffmpeg_stream->audioq.size > MAX_AUDIOQ_SIZE ||
	            ffmpeg_stream->videoq.size > MAX_VIDEOQ_SIZE ||
	            url_feof(ic->pb)) {
	        	/* wait 10 ms */
	        	SDL_Delay(10);
	        	continue;
	        }
	        if (av_read_frame(ic, pkt)) {
	        	if (url_ferror(ic->pb) == 0) {
	        	    SDL_Delay(100); /* wait for user event */
	        	    continue;
	        	} else {
	        		break;
	        	}
	        }
	        if (pkt->stream_index == ffmpeg_stream->audio_stream) {
	        	packet_queue_put(&ffmpeg_stream->audioq, pkt);
	        } else if (pkt->stream_index == ffmpeg_stream->video_stream) {
	        	packet_queue_put(&ffmpeg_stream->videoq, pkt);
	        } else {
	        	av_free_packet(pkt);
	        }
	} while (1);

out:
	pthread_mutex_lock(&ffmpeg_stream->decode_mutex);
	if (ffmpeg_stream->audio_stream >= 0) {
		__debugf("closing audio stream component");
		ffmpeg_stream_component_close(ffmpeg_stream, ffmpeg_stream->audio_stream);
	}
	if (ffmpeg_stream->video_stream >= 0) {
		__debugf("closing video stream component");
		ffmpeg_stream_component_close(ffmpeg_stream, ffmpeg_stream->video_stream);
	}
	if (ffmpeg_stream->subtitle_stream >= 0)
		ffmpeg_stream_component_close(ffmpeg_stream, ffmpeg_stream->subtitle_stream);
	if (ffmpeg_stream->ic) {
		av_close_input_file(ffmpeg_stream->ic);
		ffmpeg_stream->ic = NULL;
	}
	url_set_interrupt_cb(NULL);
	ffmpeg_stream_global = NULL;
	ffmpeg_stream->decode_stopped = 1;
	pthread_cond_signal(&ffmpeg_stream->decode_cond);
	__debugf("stopped decode");
	pthread_mutex_unlock(&ffmpeg_stream->decode_mutex);

	return NULL;
}

static int ffmpeg_uninit (void)
{
	SDL_Quit();
	return 0;
}

static int ffmpeg_init (void)
{
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
	/* register all codecs, demux and protocols */
	avcodec_register_all();
	avdevice_register_all();
	av_register_all();
	return 0;
}

static ffmpeg_stream_t * ffmpeg_stream_open (char *uri)
{
	ffmpeg_stream_t *ffmpeg_stream;

	ffmpeg_stream = (ffmpeg_stream_t *) malloc(sizeof(ffmpeg_stream_t));
	if (ffmpeg_stream == NULL) {
		return NULL;
	}
	memset(ffmpeg_stream, 0, sizeof(ffmpeg_stream_t));

	ffmpeg_stream->uri = strdup(uri);
	if (ffmpeg_stream->uri == NULL) {
		__debugf("strdup(%s) failed", uri);
		free(ffmpeg_stream);
		return NULL;
	}

	__debugf("setting decode_parameters");
	ffmpeg_stream->audio_stream = -1;
	ffmpeg_stream->video_stream = -1;
	ffmpeg_stream->subtitle_stream = -1;
	ffmpeg_stream->video_frame_width = 0;
	ffmpeg_stream->video_frame_height = 0;
	ffmpeg_stream->video_pixel_format = PIX_FMT_NONE;
	ffmpeg_stream->generatepts = 0;
	ffmpeg_stream->wanted_audio_stream = 0;
	ffmpeg_stream->wanted_video_stream = 0;
	ffmpeg_stream->audio_disable = 0;
	ffmpeg_stream->video_disable = 0;
	ffmpeg_stream->thread_count = 1;
	ffmpeg_stream->av_sync_type = AV_SYNC_AUDIO_MASTER;
	ffmpeg_stream->decoder_reorder_pts = 0;
	ffmpeg_stream->sws_flags = SWS_BICUBIC;

	pthread_mutex_init(&ffmpeg_stream->pictq_mutex, NULL);
	pthread_cond_init(&ffmpeg_stream->pictq_cond, NULL);

	list_init(&ffmpeg_stream->timer_list);
	pthread_mutex_init(&ffmpeg_stream->timer_mutex, NULL);

	list_init(&ffmpeg_stream->event_list);
	pthread_mutex_init(&ffmpeg_stream->event_mutex, NULL);

	pthread_cond_init(&ffmpeg_stream->loop_cond, NULL);
	pthread_mutex_init(&ffmpeg_stream->loop_mutex, NULL);

	pthread_cond_init(&ffmpeg_stream->decode_cond, NULL);
	pthread_mutex_init(&ffmpeg_stream->decode_mutex, NULL);

	av_init_packet(&ffmpeg_stream->flush_packet);
	ffmpeg_stream->flush_packet.data = (unsigned char *) "FLUSH";
	ffmpeg_stream->flush_packet.size = strlen((char *) ffmpeg_stream->flush_packet.data);

	pthread_mutex_lock(&ffmpeg_stream->loop_mutex);
	pthread_create(&ffmpeg_stream->loop_thread, NULL, ffmpeg_loop, ffmpeg_stream);
	while (ffmpeg_stream->loop_running != 1) {
		pthread_cond_wait(&ffmpeg_stream->loop_cond, &ffmpeg_stream->loop_mutex);
	}
	pthread_mutex_unlock(&ffmpeg_stream->loop_mutex);

	pthread_mutex_lock(&ffmpeg_stream->decode_mutex);
	pthread_create(&ffmpeg_stream->decode_thread, NULL, ffmpeg_decode, ffmpeg_stream);
	while (ffmpeg_stream->decode_running != 1) {
		pthread_cond_wait(&ffmpeg_stream->decode_cond, &ffmpeg_stream->decode_mutex);
	}
	pthread_mutex_unlock(&ffmpeg_stream->decode_mutex);

	schedule_refresh(ffmpeg_stream, 40);

	return ffmpeg_stream;
}

static int ffmpeg_stream_close (ffmpeg_stream_t *ffmpeg_stream)
{
	if (ffmpeg_stream == NULL) {
		__debugf("ffmpeg_stream is NULL");
		return 0;
	}
	__debugf("closing stream");

	pthread_mutex_lock(&ffmpeg_stream->decode_mutex);
	ffmpeg_stream->decode_running = 0;
	while (ffmpeg_stream->decode_stopped != 1) {
		pthread_cond_wait(&ffmpeg_stream->decode_cond, &ffmpeg_stream->decode_mutex);
	}
	pthread_mutex_unlock(&ffmpeg_stream->decode_mutex);
	pthread_join(ffmpeg_stream->decode_thread, NULL);

	ffmpeg_event_add(ffmpeg_stream, FFMPEG_EVENT_TYPE_QUIT, NULL);
	pthread_mutex_lock(&ffmpeg_stream->loop_mutex);
	while (ffmpeg_stream->loop_stopped != 1) {
		pthread_cond_wait(&ffmpeg_stream->loop_cond, &ffmpeg_stream->loop_mutex);
	}
	pthread_mutex_unlock(&ffmpeg_stream->loop_mutex);
	pthread_join(ffmpeg_stream->loop_thread, NULL);

	pthread_mutex_destroy(&ffmpeg_stream->decode_mutex);
	pthread_cond_destroy(&ffmpeg_stream->decode_cond);
	pthread_mutex_destroy(&ffmpeg_stream->loop_mutex);
	pthread_cond_destroy(&ffmpeg_stream->loop_cond);
	pthread_mutex_destroy(&ffmpeg_stream->event_mutex);
	pthread_mutex_destroy(&ffmpeg_stream->timer_mutex);
	free(ffmpeg_stream->uri);
	free(ffmpeg_stream);
	__debugf("closed stream");
	return 0;
}

static int render_ffmpeg_init (render_module_t *ffmpeg, int argc, char *argv[])
{
	ffmpeg_init();
	ffmpeg->data = NULL;
	return 0;
}

static int render_ffmpeg_uninit (render_module_t *ffmpeg)
{
	ffmpeg_uninit();
	return 0;
}

static int render_ffmpeg_stop (render_module_t *ffmpeg)
{
	ffmpeg_stream_t *stream;
	stream = (ffmpeg_stream_t *) ffmpeg->data;
	return ffmpeg_stream_close(stream);
}

static int render_ffmpeg_play (render_module_t *ffmpeg, char *uri)
{
	ffmpeg_stream_t *stream;
	stream = ffmpeg_stream_open(uri);
	ffmpeg->data = (void *) stream;
	return (ffmpeg->data == NULL) ? -1 : 0;
}

static int render_ffmpeg_pause (render_module_t *ffmpeg)
{
	return -1;
}

static int render_ffmpeg_seek (render_module_t *ffmpeg, long offset)
{
	return -1;
}

static int render_ffmpeg_info (render_module_t *ffmpeg, render_info_t *info)
{
	return 0;
}

render_module_t render_ffmpeg = {
	.author = "Alper Akcan",
	.description = "FFmpeg Rendering Plugin",
	.name = "ffmpeg",
	.init = render_ffmpeg_init,
	.uninit = render_ffmpeg_uninit,
	.play = render_ffmpeg_play,
	.seek = render_ffmpeg_seek,
	.pause = render_ffmpeg_pause,
	.stop = render_ffmpeg_stop,
	.info = render_ffmpeg_info,
};
