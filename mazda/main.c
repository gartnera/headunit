#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <linux/input.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <poll.h>



#include "hu_uti.h"
#include "hu_aap.h"

#define EVENT_DEVICE_TS    "/dev/input/filtered-touchscreen0"
#define EVENT_DEVICE_CMD   "/dev/input/event1"
#define EVENT_TYPE      EV_ABS
#define EVENT_CODE_X    ABS_X
#define EVENT_CODE_Y    ABS_Y

#define HMI_BUS_ADDRESS "unix:path=/tmp/dbus_hmi_socket"
#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

__asm__(".symver realpath1,realpath1@GLIBC_2.11.1");


typedef struct {
	GMainLoop *loop;
	GstPipeline *pipeline;
	GstAppSrc *src;
	GstElement *sink;
	GstElement *decoder;
	GstElement *queue;
	guint sourceid;
} gst_app_t;


static gst_app_t gst_app;

GstElement *mic_pipeline, *mic_sink;
GstElement *aud_pipeline, *aud_src;
GstElement *au1_pipeline, *au1_src;

GAsyncQueue *sendqueue;

typedef struct {
	int fd;
	int x;
	int y;
	uint8_t action;
	int action_recvd;
} mytouchscreen;

mytouchscreen mTouch = (mytouchscreen){0,0,0,0,0};

typedef struct {
	int fd;
	uint8_t action;
} mycommander;

mycommander mCommander = (mycommander){0,0};


typedef struct {
    int retry;
    int chan;
    int cmd_len; 
    unsigned char *cmd_buf; 
	int shouldFree;
} send_arg;

void queueSend(int retry, int chan, unsigned char* cmd_buf, int cmd_len, int shouldFree)
{
	send_arg* cmd = malloc(sizeof(send_arg));

	cmd->retry = retry;
	cmd->chan = chan;
	cmd->cmd_buf = cmd_buf;
	cmd->cmd_len = cmd_len;
	cmd->shouldFree = shouldFree;

	g_async_queue_push(sendqueue, cmd);
}


int mic_change_state = 0;

static void read_mic_data (GstElement * sink);

static gboolean read_data(gst_app_t *app)
{
	GstBuffer *buffer;
	guint8 *ptr;
	GstFlowReturn ret;
	int iret;
	char *vbuf;
	char *abuf;
	int res_len = 0;

	iret = hu_aap_recv_process ();                       

	if (iret != 0) {
		printf("hu_aap_recv_process() iret: %d\n", iret);
		g_main_loop_quit(app->loop);		
		return FALSE;
	}

	/* Is there a video buffer queued? */
	vbuf = vid_read_head_buf_get (&res_len);

	if (vbuf != NULL) {

		//buffer = gst_buffer_new();
		//gst_buffer_set_data(buffer, vbuf, res_len);
		buffer = gst_buffer_new_and_alloc(res_len);
		memcpy(GST_BUFFER_DATA(buffer),vbuf,res_len);

		ret = gst_app_src_push_buffer(app->src, buffer);

		if(ret !=  GST_FLOW_OK){
			printf("push buffer returned %d for %d bytes \n", ret, res_len);
			return FALSE;
		}
	}
	
	/* Is there an audio buffer queued? */
	abuf = aud_read_head_buf_get (&res_len);
	if (abuf != NULL) {

		//buffer = gst_buffer_new();
		//gst_buffer_set_data(buffer, abuf, res_len);
		
		buffer = gst_buffer_new_and_alloc(res_len);
		memcpy(GST_BUFFER_DATA(buffer),abuf,res_len);

		if (res_len <= 2048 + 96)
			ret = gst_app_src_push_buffer((GstAppSrc *)au1_src, buffer);
		else
			ret = gst_app_src_push_buffer((GstAppSrc *)aud_src, buffer);

		if(ret !=  GST_FLOW_OK){
			printf("push buffer returned %d for %d bytes \n", ret, res_len);
			return FALSE;
		}
	}	

	return TRUE;
}

static int shouldRead = FALSE;

static void start_feed (GstElement * pipeline, guint size, void *app)
{
	shouldRead = TRUE;
}

static void stop_feed (GstElement * pipeline, void *app)
{
	shouldRead = FALSE;
}


static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
	gst_app_t *app = (gst_app_t*)ptr;

	switch(GST_MESSAGE_TYPE(message)){

		case GST_MESSAGE_ERROR:{
					       gchar *debug;
					       GError *err;

					       gst_message_parse_error(message, &err, &debug);
					       g_print("Error %s\n", err->message);
					       g_error_free(err);
					       g_free(debug);
					       g_main_loop_quit(app->loop);
				       }
				       break;

		case GST_MESSAGE_WARNING:{
						 gchar *debug;
						 GError *err;
						 gchar *name;

						 gst_message_parse_warning(message, &err, &debug);
						 g_print("Warning %s\nDebug %s\n", err->message, debug);

						 name = (gchar *)GST_MESSAGE_SRC_NAME(message);

						 g_print("Name of src %s\n", name ? name : "nil");
						 g_error_free(err);
						 g_free(debug);
					 }
					 break;

		case GST_MESSAGE_EOS:
					 g_print("End of stream\n");
					 g_main_loop_quit(app->loop);
					 break;

		case GST_MESSAGE_STATE_CHANGED:
					 break;

		default:
//					 g_print("got message %s\n", \
							 gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
					 break;
	}

	return TRUE;
}

static int gst_pipeline_init(gst_app_t *app)
{
	GstBus *bus;
	GstStateChangeReturn state_ret;
	
	GError *error = NULL;

	gst_init(NULL, NULL);

//	app->pipeline = (GstPipeline*)gst_parse_launch("appsrc name=mysrc is-live=true block=false max-latency=1000000 ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 ! mfw_v4lsink max-lateness=1000000000 sync=false async=false", &error);

    app->pipeline = (GstPipeline*)gst_parse_launch("appsrc name=mysrc is-live=true block=false max-latency=1000000 ! h264parse ! vpudec low-latency=true framedrop=true framedrop-level-mask=0x200 ! mfw_isink name=mysink axis-left=0 axis-top=0 disp-width=800 disp-height=480 max-lateness=1000000000 sync=false async=false", &error);
		
	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	
	bus = gst_pipeline_get_bus(app->pipeline);
	gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);
	gst_object_unref(bus);

	app->src = (GstAppSrc*)gst_bin_get_by_name (GST_BIN (app->pipeline), "mysrc");
	app->sink = (GstElement*)gst_bin_get_by_name (GST_BIN (app->pipeline), "mysink");
	
	gst_app_src_set_stream_type(app->src, GST_APP_STREAM_TYPE_STREAM);

	g_signal_connect(app->src, "need-data", G_CALLBACK(start_feed), app);
		
	g_signal_connect(app->src, "enough-data", G_CALLBACK(stop_feed), app);

	aud_pipeline = gst_parse_launch("appsrc name=audsrc is-live=true block=false max-latency=1000000 ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=48000, channels=2 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	aud_src = gst_bin_get_by_name (GST_BIN (aud_pipeline), "audsrc");
	
	gst_app_src_set_stream_type((GstAppSrc *)aud_src, GST_APP_STREAM_TYPE_STREAM);


	au1_pipeline = gst_parse_launch("appsrc name=au1src is-live=true block=false max-latency=1000000 ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, rate=16000, channels=1 ! volume volume=0.4 ! alsasink ",&error);

	if (error != NULL) {
		printf("could not construct pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}	

	au1_src = gst_bin_get_by_name (GST_BIN (au1_pipeline), "au1src");
	
	gst_app_src_set_stream_type((GstAppSrc *)au1_src, GST_APP_STREAM_TYPE_STREAM);



	mic_pipeline = gst_parse_launch("alsasrc name=micsrc ! audioconvert ! audio/x-raw-int, signed=true, endianness=1234, depth=16, width=16, channels=1, rate=16000 ! queue !appsink name=micsink async=false emit-signals=true blocksize=8192",&error);
	
	if (error != NULL) {
		printf("could not construct mic pipeline: %s\n", error->message);
		g_clear_error (&error);	
		return -1;
	}
	
	mic_sink = gst_bin_get_by_name (GST_BIN (mic_pipeline), "micsink");

	g_object_set(G_OBJECT(mic_sink), "throttle-time", 3000000, NULL);
		
	g_signal_connect(mic_sink, "new-buffer", G_CALLBACK(read_mic_data), NULL);
	
	state_ret = gst_element_set_state (mic_pipeline, GST_STATE_READY);

	return 0;

}

/*
static int aa_cmd_send(int cmd_len, unsigned char *cmd_buf, int res_max, unsigned char *res_buf)
{
	int chan = cmd_buf[0];
	int res_len = 0;
	int ret = 0;

	ret = hu_aap_enc_send (0, chan, cmd_buf+4, cmd_len - 4);
	if (ret < 0) {
		printf("aa_cmd_send(): hu_aap_enc_send() failed with (%d)\n", ret);
		return ret;
	}
	
	return ret;

} */

static size_t uleb128_encode(uint64_t value, uint8_t *data)
{
	uint8_t cbyte;
	size_t enc_size = 0;

	do {
		cbyte = value & 0x7f;
		value >>= 7;
		if (value != 0)
			cbyte |= 0x80;
		data[enc_size++] = cbyte;
	} while (value != 0);

	return enc_size;
}

static size_t varint_encode (uint64_t val, uint8_t *ba, int idx) {
	
	if (val >= 0x7fffffffffffffff) {
		return 1;
	}

	uint64_t left = val;
	int idx2 = 0;
	
	for (idx2 = 0; idx2 < 9; idx2 ++) {
		ba [idx+idx2] = (uint8_t) (0x7f & left);
		left = left >> 7;
		if (left == 0) {
			return (idx2 + 1);
		}
		else if (idx2 < 9 - 1) {
			ba [idx+idx2] |= 0x80;
		}
	}
	
	return 9;
}

#define ACTION_DOWN	0
#define ACTION_UP	1
#define ACTION_MOVE	2
#define TS_MAX_REQ_SZ	32
static const uint8_t ts_header[] ={0x80, 0x01, 0x08};
static const uint8_t ts_sizes[] = {0x1a, 0x09, 0x0a, 0x03};
static const uint8_t ts_footer[] = {0x10, 0x00, 0x18};

static void aa_touch_event(uint8_t action, int x, int y) {
	struct timespec tp;
	uint8_t *buf;
	int idx;
	int siz_arr = 0;
	int size1_idx, size2_idx, i;
	int axis = 0;
	int coordinates[3] = {x, y, 0};
	int ret;

	buf = (uint8_t *)malloc(TS_MAX_REQ_SZ);
	if(!buf) {
		printf("Failed to allocate touchscreen event buffer\n");
		return;
	}

	/* Fetch the time stamp */
	clock_gettime(CLOCK_REALTIME, &tp);

	/* Copy header */
	memcpy(buf, ts_header, sizeof(ts_header));
//	idx = sizeof(ts_header) +
//	      uleb128_encode(tp.tv_nsec, buf + sizeof(ts_header));

	idx = sizeof(ts_header) +
	      varint_encode(tp.tv_sec * 1000000000 +tp.tv_nsec, buf + sizeof(ts_header),0);

	size1_idx = idx + 1;
	size2_idx = idx + 3;

	/* Copy sizes */
	memcpy(buf+idx, ts_sizes, sizeof(ts_sizes));
	idx += sizeof(ts_sizes);

	/* Set magnitude of each axis */
	for (i=0; i<3; i++) {
		axis += 0x08;
		buf[idx++] = axis;
		/* FIXME The following can be optimzed to update size1/2 at end of loop */
		siz_arr = uleb128_encode(coordinates[i], &buf[idx]);
		idx += siz_arr;
		buf[size1_idx] += siz_arr;
		buf[size2_idx] += siz_arr;
	}

	/* Copy footer */
	memcpy(buf+idx, ts_footer, sizeof(ts_footer));
	idx += sizeof(ts_footer);

	buf[idx++] = action;

	queueSend(0, AA_CH_TOU, buf, idx, TRUE);
	
}

static size_t uptime_encode(uint64_t value, uint8_t *data)
{

	int ctr = 0;
	for (ctr = 7; ctr >= 0; ctr --) {                           // Fill 8 bytes backwards
		data [6 + ctr] = (uint8_t)(value & 0xFF);
		value = value >> 8;
	}

	return 8;
}

static const uint8_t mic_header[] ={0x00, 0x00};
static const int max_size = 8192;


static void read_mic_data (GstElement * sink)
{		
	GstBuffer *gstbuf;
	int ret;
	
	g_signal_emit_by_name (sink, "pull-buffer", &gstbuf,NULL);
	
	if (gstbuf) {

		struct timespec tp;

		/* if mic is stopped, don't bother sending */	

		if (mic_change_state == 0) {
			printf("Mic stopped.. dropping buffers \n");
			gst_buffer_unref(gstbuf);
			return;
		}
		
		/* Fetch the time stamp */
		clock_gettime(CLOCK_REALTIME, &tp);
		
		gint mic_buf_sz;
		mic_buf_sz = GST_BUFFER_SIZE (gstbuf);
		
		int idx;
		
		if (mic_buf_sz <= 64) {
			printf("Mic data < 64 \n");
			return;
		}
		
		uint8_t *mic_buffer = (uint8_t *)malloc(14 + mic_buf_sz);
		
		/* Copy header */
		memcpy(mic_buffer, mic_header, sizeof(mic_header));
		
		idx = sizeof(mic_header) + uptime_encode(tp.tv_nsec * 0.001, mic_buffer);

		/* Copy PCM Audio Data */
		memcpy(mic_buffer+idx, GST_BUFFER_DATA(gstbuf), mic_buf_sz);
		idx += mic_buf_sz;
		
		queueSend(1, AA_CH_MIC, mic_buffer, idx, TRUE);
	
		gst_buffer_unref(gstbuf);
	}
} 

int nightmode = 0;

gboolean touch_poll_event(gpointer data)
{
	int mic_ret = hu_aap_mic_get ();
	
	if (mic_change_state == 0 && mic_ret == 2) {
		printf("SHAI1 : Mic Started\n");
		mic_change_state = 2;
		gst_element_set_state (mic_pipeline, GST_STATE_PLAYING);
	}
		
	if (mic_change_state == 2 && mic_ret == 1) {
		printf("SHAI1 : Mic Stopped\n");
		mic_change_state = 0;
		gst_element_set_state (mic_pipeline, GST_STATE_READY);
	}	
	
	struct input_event event[64];
	const size_t ev_size = sizeof(struct input_event);
	const size_t buffer_size = ev_size * 64;
    ssize_t size;
    gst_app_t *app = (gst_app_t *)data;
	
	fd_set set;
	struct timeval timeout;
	int unblocked;

	FD_ZERO(&set);
	FD_SET(mTouch.fd, &set);

	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;
	
	unblocked = select(mTouch.fd + 1, &set, NULL, NULL, &timeout);

	if (unblocked == -1) {
		printf("Error in read...\n");
		g_main_loop_quit(app->loop);
		return FALSE;
	}
	else if (unblocked == 0) {
			return TRUE;
	}
	
	size = read(mTouch.fd, &event, buffer_size);
	
	if (size == 0 || size == -1)
		return FALSE;
	
	if (size < ev_size) {
		printf("Error size when reading\n");
		g_main_loop_quit(app->loop);
		return FALSE;
	}
	
	int num_chars = size / ev_size;
	
	int i;
	for (i=0;i < num_chars;i++) {
		switch (event[i].type) {
			case EV_ABS:
				switch (event[i].code) {
					case ABS_MT_POSITION_X:
						mTouch.x = event[i].value * 800/4095;
						break;
					case ABS_MT_POSITION_Y:
						mTouch.y = event[i].value * 480/4095;
						break;
				}
				break;
			case EV_KEY:
				if (event[i].code == BTN_TOUCH) {
					mTouch.action_recvd = 1;
					if (event[i].value == 1) {
						mTouch.action = ACTION_DOWN;
					}
					else {
						mTouch.action = ACTION_UP;
					}
				}
				break;
			case EV_SYN:
				if (mTouch.action_recvd == 0) {
					mTouch.action = ACTION_MOVE;
					aa_touch_event(mTouch.action, mTouch.x, mTouch.y);
				} else {
					aa_touch_event(mTouch.action, mTouch.x, mTouch.y);
					mTouch.action_recvd = 0;
				}
				break;
		}
	} 

	
	return TRUE;
}

GMainLoop *mainloop;

int displayStatus = 1;

inline int dbus_message_decode_timeval(DBusMessageIter *iter, struct timeval *time)
{
	DBusMessageIter sub;
	uint64_t tv_sec = 0;
	uint64_t tv_usec = 0;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRUCT) {
		return TRUE;
	}

	dbus_message_iter_recurse(iter, &sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT64) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &tv_sec);
	dbus_message_iter_next(&sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT64) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &tv_usec);

	dbus_message_iter_next(iter);

	time->tv_sec = tv_sec;
	time->tv_usec = tv_usec;

	return TRUE;
}
inline int dbus_message_decode_input_event(DBusMessageIter *iter, struct input_event *event)
{
	DBusMessageIter sub;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRUCT) {
		return TRUE;
	}

	dbus_message_iter_recurse(iter, &sub);

	if (!dbus_message_decode_timeval(&sub, &event->time)) {
		return FALSE;
	}

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT16) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &event->type);
	dbus_message_iter_next(&sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_UINT16) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &event->code);
	dbus_message_iter_next(&sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INT32) {
		return FALSE;
	}
	dbus_message_iter_get_basic(&sub, &event->value);

	dbus_message_iter_next(iter);

	return TRUE;
}

static gboolean delayedShouldDisplayTrue(gpointer data)
{
	gst_app_t *app = &gst_app;
	g_object_set(G_OBJECT(app->sink), "should-display", TRUE, NULL);
	displayStatus = TRUE;

	return FALSE;
}

typedef enum
{
    HUIB_UP = 0x13,
    HUIB_DOWN = 0x14,
    HUIB_LEFT = 0x15,
    HUIB_RIGHT = 0x16,
    HUIB_BACK = 0x04,
    HUIB_ENTER = 0x17,
    HUIB_MIC = 0x54,
    HUIB_PLAYPAUSE = 0x55,
    HUIB_NEXT = 0x57,
    HUIB_PREV = 0x58,
    HUIB_PHONE = 0x5,
    HUIB_START = 126,
    HUIB_STOP = 127,
    
}  HU_INPUT_BUTTON;


static int hu_fill_button_message(uint8_t* buffer, uint64_t timeStamp, HU_INPUT_BUTTON button, int isPress)
{
    int buffCount = 0;
    buffer[buffCount++] = 0x80;
    buffer[buffCount++] = 0x01;
    buffer[buffCount++] = 0x08;
   
    buffCount += varint_encode(timeStamp, buffer + buffCount, 0);
    
    buffer[buffCount++] = 0x22;
    buffer[buffCount++] = 0x0A;
    buffer[buffCount++] = 0x0A;
    buffer[buffCount++] = 0x08;
    buffer[buffCount++] = 0x08;
    buffer[buffCount++] = (uint8_t)button;
    buffer[buffCount++] = 0x10;
    buffer[buffCount++] = isPress ? 0x01 : 0x00;
    buffer[buffCount++] = 0x18;
    buffer[buffCount++] = 0x00;
    buffer[buffCount++] = 0x20;
    buffer[buffCount++] = 0x00;
    return buffCount;
}


static DBusHandlerResult handle_dbus_message(DBusConnection *c, DBusMessage *message, void *p)
{
    struct timespec tp;
	gst_app_t *app = &gst_app;
	DBusMessageIter iter;

	if (strcmp("KeyEvent", dbus_message_get_member(message)) == 0)
	{
		struct input_event event;

		dbus_message_iter_init(message, &iter);
		dbus_message_decode_input_event(&iter, &event);

		//key press
		if (event.type == EV_KEY && (event.value == 1 || event.value == 0)) {
            
            clock_gettime(CLOCK_REALTIME, &tp);
            uint64_t timestamp = tp.tv_sec * 1000000000 +tp.tv_nsec;
            uint8_t* keyTempBuffer = 0;
            int keyTempSize = 0;
            
            printf("Key code %i value %i\n", (int)event.code, (int)event.value);
			switch (event.code) {
				case KEY_G:
                    printf("KEY_G\n");
                    keyTempBuffer = malloc(512);
                    keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_MIC, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                //Make the music button play/pause
                case KEY_E:
                    printf("KEY_E\n");
                    keyTempBuffer = malloc(512);
                    keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_PLAYPAUSE, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
				case KEY_LEFTBRACE:
                    printf("KEY_LEFTBRACE\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_NEXT, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
				case KEY_RIGHTBRACE:
                    printf("KEY_RIGHTBRACE\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_PREV, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                case KEY_BACKSPACE:
                    printf("KEY_BACKSPACE\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_BACK, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                case KEY_ENTER:
                    printf("KEY_ENTER\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_ENTER, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                case KEY_LEFT:
                case KEY_N:
                    printf("KEY_LEFT\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_LEFT, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                case KEY_RIGHT:
                case KEY_M:
                    printf("KEY_RIGHT\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_RIGHT, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                case KEY_UP:
                    printf("KEY_UP\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_UP, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
                case KEY_DOWN:
                    printf("KEY_DOWN\n");
                    keyTempBuffer = malloc(512);
					keyTempSize = hu_fill_button_message(keyTempBuffer, timestamp, HUIB_DOWN, event.value == 1);
                    queueSend (0,AA_CH_TOU, keyTempBuffer, keyTempSize, TRUE);
					break;
				case KEY_HOME:
                    printf("KEY_HOME\n");
                    if (event.value == 1)
                    {
                        g_main_loop_quit (mainloop);
                    }
					break;
				case KEY_R:
                    printf("KEY_R\n");
                    if (event.value == 1)
                    {
                        if (displayStatus)
                        {
                            g_object_set(G_OBJECT(app->sink), "should-display", FALSE, NULL);
                            displayStatus = FALSE;
                        }
                        else
                        {
                            g_object_set(G_OBJECT(app->sink), "should-display", TRUE, NULL);
                            displayStatus = TRUE;
                        }
                    }
					break;
			}
		}
		//key release
		//else if (event.type == EV_KEY && event.value == 1)

	}
	else if(strcmp("DisplayMode", dbus_message_get_member(message)) == 0)
	{
		int displayMode;
		if (dbus_message_iter_init(message, &iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32)
		{
			dbus_message_iter_get_basic(&iter, &displayMode);
			if (displayMode)
			{
				g_object_set(G_OBJECT(app->sink), "should-display", FALSE, NULL);
				displayStatus = FALSE;
			}
			else
			{
				g_timeout_add(750, delayedShouldDisplayTrue, NULL);
			}
		}

	}

	dbus_message_unref(message);
	return DBUS_HANDLER_RESULT_HANDLED;

}

static void * input_thread(void *app) {

	DBusConnection *hmi_bus;
	DBusError error;

	hmi_bus = dbus_connection_open(HMI_BUS_ADDRESS, &error);

	if (!hmi_bus) {
		printf("DBUS: failed to connect to HMI bus: %s: %s\n", error.name, error.message);
	}

	if (!dbus_bus_register(hmi_bus, &error)) {
		printf("DBUS: failed to register with HMI bus: %s: %s\n", error.name, error.message);
	}

	dbus_connection_add_filter(hmi_bus, handle_dbus_message, NULL, NULL);
	dbus_bus_add_match(hmi_bus, "type='signal',interface='us.insolit.mazda.connector',member='KeyEvent'", &error);
	dbus_bus_add_match(hmi_bus, "type='signal',interface='com.jci.bucpsa',member='DisplayMode'", &error);

	while (touch_poll_event(app)) {
		//commander_poll_event(app);		
		dbus_connection_read_write_dispatch(hmi_bus, 100);
		//ms_sleep(100);
	}
}

static void * nightmode_thread(void *app) 
{

	// Initialize HMI bus
	DBusConnection *hmi_bus;
	DBusError error;

	hmi_bus = dbus_connection_open(HMI_BUS_ADDRESS, &error);

	if (!hmi_bus) {
		printf("DBUS: failed to connect to HMI bus: %s: %s\n", error.name, error.message);
	}

	if (!dbus_bus_register(hmi_bus, &error)) {
		printf("DBUS: failed to register with HMI bus: %s: %s\n", error.name, error.message);
	}

	// Wait for mainloop to start
	ms_sleep(100);

	while (g_main_loop_is_running (mainloop)) {

		DBusMessage *msg = dbus_message_new_method_call("com.jci.BLM_TIME", "/com/jci/BLM_TIME", "com.jci.BLM_TIME", "GetClock");
		DBusPendingCall *pending = NULL;

		if (!msg) {
			printf("DBUS: failed to create message \n");
		}

		if (!dbus_connection_send_with_reply(hmi_bus, msg, &pending, -1)) {
			printf("DBUS: failed to send message \n");
		}

		dbus_connection_flush(hmi_bus);
		dbus_message_unref(msg);

		dbus_pending_call_block(pending);
		msg = dbus_pending_call_steal_reply(pending);
		if (!msg) {
			printf("DBUS: received null reply \n");
		}

		dbus_uint32_t nm_hour;
		dbus_uint32_t nm_min;
		dbus_uint32_t nm_timestamp;
		dbus_uint64_t nm_calltimestamp;
		if (!dbus_message_get_args(msg, &error, DBUS_TYPE_UINT32, &nm_hour,
					DBUS_TYPE_UINT32, &nm_min,
					DBUS_TYPE_UINT32, &nm_timestamp,
					DBUS_TYPE_UINT64, &nm_calltimestamp,
					DBUS_TYPE_INVALID)) {
			printf("DBUS: failed to get result %s: %s\n", error.name, error.message);
		}

		dbus_message_unref(msg);

		int nightmodenow = 1;

		if (nm_hour >= 6 && nm_hour <= 18)
			nightmodenow = 0;

		if (nightmode != nightmodenow) {
			nightmode = nightmodenow;
			byte* rspds = malloc(sizeof(byte) * 6);
			rspds[0] = -128; 
			rspds[1] = 0x03;
			rspds[2] = 0x52; 
			rspds[3] = 0x02;
			rspds[4] = 0x08;
			if (nightmode == 0)
				rspds[5]= 0x00;
			else
				rspds[5] = 0x01;

			queueSend(0,AA_CH_SEN, rspds, sizeof (byte) * 6, TRUE); 	// Send Sensor Night mode
		}

		sleep(600);		
	}
}



gboolean myMainLoop(gpointer app)
{
	if (shouldRead)
	{
		read_data(app);
	}

	send_arg* cmd;

	if (cmd = g_async_queue_try_pop(sendqueue))
	{
		hu_aap_enc_send(cmd->retry, cmd->chan, cmd->cmd_buf, cmd->cmd_len);
		if(cmd->shouldFree)
			free(cmd->cmd_buf);
		free(cmd);
	}

	return TRUE; 
}

static void * main_thread(void *app) {

	ms_sleep(100);

	while (mainloop && g_main_loop_is_running (mainloop)) {
		myMainLoop(app);
	}
}


static int gst_loop(gst_app_t *app)
{
	int ret;
	GstStateChangeReturn state_ret;

	state_ret = gst_element_set_state((GstElement*)app->pipeline, GST_STATE_PLAYING);
	state_ret = gst_element_set_state((GstElement*)aud_pipeline, GST_STATE_PLAYING);
	state_ret = gst_element_set_state((GstElement*)au1_pipeline, GST_STATE_PLAYING);

	//	g_warning("set state returned %d\n", state_ret);

	app->loop = g_main_loop_new (NULL, FALSE);

	mainloop = app->loop;

	//	g_timeout_add_full(G_PRIORITY_HIGH, 1, myMainLoop, (gpointer)app, NULL);

	printf("Starting Android Auto...\n");
	g_main_loop_run (app->loop);

	// TO-DO
	state_ret = gst_element_set_state((GstElement*)app->pipeline, GST_STATE_NULL);
	//	g_warning("set state null returned %d\n", state_ret);

	gst_object_unref(app->pipeline);
	gst_object_unref(mic_pipeline);
	gst_object_unref(aud_pipeline);
	gst_object_unref(au1_pipeline);

	return ret;
}

static void signals_handler (int signum)
{
	if (signum == SIGINT)
	{
		if (mainloop && g_main_loop_is_running (mainloop))
		{
			g_main_loop_quit (mainloop);
		}
	}
}

int main (int argc, char *argv[])
{	
	signal (SIGTERM, signals_handler);

	gst_app_t *app = &gst_app;
	int ret = 0;
	errno = 0;
	byte ep_in_addr  = -2;
	byte ep_out_addr = -2;

	/* Start AA processing */
	ret = hu_aap_start (ep_in_addr, ep_out_addr);
	if (ret == -1)
	{
		printf("Phone switched to accessory mode. Attempting once more.\n");
		sleep(1);
		ret = hu_aap_start (ep_in_addr, ep_out_addr);
	}

	if (ret < 0) {
		if (ret == -2)
		{
			printf("Phone is not connected. Connect a supported phone and restart.\n");
			return 0;
		}
		else if (ret == -1)
			printf("Phone switched to accessory mode. Restart to enter AA mode.\n");
		else
			printf("hu_app_start() ret: %d\n", ret);
		return (ret);
	}

	printf("Starting Android Auto...\n");

	/* Init gstreamer pipeline */
	ret = gst_pipeline_init(app);
	if (ret < 0) {
		printf("gst_pipeline_init() ret: %d\n", ret);
		return (-4);
	}

	/* Open Touchscreen Device */
	mTouch.fd = open(EVENT_DEVICE_TS, O_RDONLY);

	if (mTouch.fd == -1) {
		fprintf(stderr, "%s is not a vaild device\n", EVENT_DEVICE_TS);
		return -3;
	}

	sendqueue = g_async_queue_new();

	pthread_t iput_thread;

	pthread_create(&iput_thread, NULL, &input_thread, (void *)app);

	pthread_t nm_thread;

	pthread_create(&nm_thread, NULL, &nightmode_thread, (void *)app);


	pthread_t mn_thread;

	pthread_create(&mn_thread, NULL, &main_thread, (void *)app);

	/* Start gstreamer pipeline and main loop */
	ret = gst_loop(app);
	if (ret < 0) {
		printf("gst_loop() ret: %d\n", ret);
		ret = -5;
	}

	/* Stop AA processing */
	ret = hu_aap_stop ();
	if (ret < 0) {
		printf("hu_aap_stop() ret: %d\n", ret);
		ret = -6;
	}

	close(mTouch.fd);

	pthread_cancel(nm_thread);
	pthread_cancel(mn_thread);
	pthread_cancel(iput_thread);
	system("kill -SIGUSR2 $(pgrep input_filter)");

	printf("END \n");

	return 0;
}
