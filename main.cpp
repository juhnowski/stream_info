#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "opencv2/opencv.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/video.hpp"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/error.h>
    #include <libswscale/swscale.h>
}

#undef av_err2str
#define av_err2str(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)

using namespace std;
using namespace cv;

static void logging(const char *fmt, ...);

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);
static AVFrame opencv_frame(AVFrame *pFrame );
static char *sdp_filename = "test.sdp";
static cv::Mat avframe_to_cvmat(AVFrame *frame, AVCodecContext *pCodecCtx);
static AVFrame cvmat_to_avframe(cv::Mat* frame);
static AVFrame* ProcessFrame(AVFrame *frame, AVCodecContext *pCodecCtx);
static AVFrame opencv_frame(AVFrame *frame, AVCodecContext *pCodecCtx );

int response = 0;
int how_many_packets_to_process = 200;


int main(int argc, const char *argv[]) {

    if (argc < 4) {
        std::cout << "Usage: ./stream_info video_port audio_port rtmp_url" << std::endl;
        return -1;
    }
//----------------------------------------------------------------------------------------------------------
    logging("initializing all the containers, codecs and protocols.");

    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        logging("ERROR could not allocate memory for Format Context");
        return -1;
    }
//----------------------------------------------------------------------------------------------------------
    logging("opening the input file (%s) and loading format (container) header", sdp_filename);

    /**
     * создаем словарь с белым списком
     */

    AVDictionary *d = NULL;
    av_dict_set(&d, "protocol_whitelist", "file,rtp,udp",0);

    /**
     * Открываем поток
     * Читаем заголовок
     * Кодек не открыт
     */
    if (avformat_open_input(&pFormatContext, sdp_filename, NULL, &d) != 0) {
        logging("ERROR could not open the file");
        return -1;
    }

//----------------------------------------------------------------------------------------------------------
    logging("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);
    logging("finding stream info from format");

    /**
     * Получаем информацию о стримах
     * Функция заполняет pFormatContext->streams
     */
    if (avformat_find_stream_info(pFormatContext,  NULL) < 0) {
        logging("ERROR could not get the stream info");
        return -1;
    }
//----------------------------------------------------------------------------------------------------------
    /**
     * Структура данных по кодеку
     */
    AVCodec *pCodec = NULL;

    /**
     * Параметры кодека
     */
    AVCodecParameters *pCodecParameters =  NULL;

    int video_stream_index = -1;


    /**
     * По каждому стриму получаем данные
LOG: finding stream info from format
LOG: AVStream->time_base before open coded 1/90000
LOG: AVStream->r_frame_rate before open coded 24/1
LOG: AVStream->start_time 3750
LOG: AVStream->duration -9223372036854775808
LOG: finding the proper decoder (CODEC)
LOG: Video Codec: resolution 1280 x 720
LOG: 	Codec h264 ID 27 bit_rate 0
LOG: AVStream->time_base before open coded 1/44100
LOG: AVStream->r_frame_rate before open coded 0/0
LOG: AVStream->start_time 0
LOG: AVStream->duration -9223372036854775808
LOG: finding the proper decoder (CODEC)
LOG: Audio Codec: 2 channels, sample rate 44100
LOG: 	Codec aac ID 86018 bit_rate 0
     */
    for (int i = 0; i < pFormatContext->nb_streams; i++)
    {
        AVCodecParameters *pLocalCodecParameters =  NULL;
        pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
        logging("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
        logging("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
        logging("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
        logging("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);

        logging("finding the proper decoder (CODEC)");

        AVCodec *pLocalCodec = NULL;

        // finds the registered decoder for a codec ID
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga19a0ca553277f019dd5b0fec6e1f9dca
        pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

        if (pLocalCodec==NULL) {
            logging("ERROR unsupported codec!");
            return -1;
        }

        // when the stream is a video we store its index, codec parameters and codec
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (video_stream_index == -1) {
                video_stream_index = i;
                pCodec = pLocalCodec;
                pCodecParameters = pLocalCodecParameters;
            }

            logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width, pLocalCodecParameters->height);
        } else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            logging("Audio Codec: %d channels, sample rate %d", pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
        }

        // print its name, id and bitrate
        logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name, pLocalCodec->id, pLocalCodecParameters->bit_rate);
    }
//----------------------------------------------------------------------------------------------------------
    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext)
    {
        logging("failed to allocated memory for AVCodecContext");
        return -1;
    }
//----------------------------------------------------------------------------------------------------------
// Устанавливаем параметры кодека
    if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0)
    {
        logging("failed to copy codec params to codec context");
        return -1;
    }
//----------------------------------------------------------------------------------------------------------
// Инициализируем AVCodecContext для испльзования выбранного AVCodec.
    if (avcodec_open2(pCodecContext, pCodec, NULL) < 0)
    {
        logging("failed to open codec through avcodec_open2");
        return -1;
    }
//----------------------------------------------------------------------------------------------------------
// Выделяем память под фрейм и пакет
    // https://ffmpeg.org/doxygen/trunk/structAVFrame.html
    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame)
    {
        logging("failed to allocated memory for AVFrame");
        return -1;
    }
    // https://ffmpeg.org/doxygen/trunk/structAVPacket.html
    AVPacket *pPacket = av_packet_alloc();
    if (!pPacket)
    {
        logging("failed to allocated memory for AVPacket");
        return -1;
    }
//----------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------
// Заполняем пакет данными из стрима
    while (av_read_frame(pFormatContext, pPacket) >= 0)
    {
        // if it's the video stream
        if (pPacket->stream_index == video_stream_index) {
            logging("AVPacket->pts %" PRId64, pPacket->pts);
            response = decode_packet(pPacket, pCodecContext, pFrame);
            if (response < 0)
                break;
            // stop it, otherwise we'll be saving hundreds of frames
            if (--how_many_packets_to_process <= 0) break;
        }
        // https://ffmpeg.org/doxygen/trunk/group__lavc__packet.html#ga63d5a489b419bd5d45cfd09091cbcbc2
        av_packet_unref(pPacket);
    }

    logging("releasing all the resources");

    avformat_close_input(&pFormatContext);
    av_packet_free(&pPacket);
    av_frame_free(&pFrame);
    avcodec_free_context(&pCodecContext);

    return 0;
}

static void logging(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "LOG: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame)
{
    // Supply raw packet data as input to a decoder
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
    int response = avcodec_send_packet(pCodecContext, pPacket);

    if (response < 0) {
        logging("Error while sending a packet to the decoder: %s", response);
        return response;
    }

    while (response >= 0)
    {
        // Return decoded output data (into a frame) from a decoder
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c
        response = avcodec_receive_frame(pCodecContext, pFrame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } else if (response < 0) {
            logging("Error while receiving a frame from the decoder: %s", response);
            return response;
        }

        if (response >= 0) {
            logging(
                    "Frame %d (type=%c, size=%d bytes) pts %d key_frame %d [DTS %d]",
                    pCodecContext->frame_number,
                    av_get_picture_type_char(pFrame->pict_type),
                    pFrame->pkt_size,
                    pFrame->pts,
                    pFrame->key_frame,
                    pFrame->coded_picture_number
            );

            char frame_filename[1024];
            char tmp_filename[1024];
            snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", pCodecContext->frame_number);
            snprintf(tmp_filename, sizeof(frame_filename), "%s-%d_tmp.pgm", "frame", pCodecContext->frame_number);
            // save a grayscale frame into a .pgm file
            AVFrame t = opencv_frame(pFrame, pCodecContext);
            AVFrame *tmp = &t;
        //    AVFrame *tmp = ProcessFrame(tmp, pCodecContext);
            save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
            save_gray_frame(tmp->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, tmp_filename);
            cout<<frame_filename<<endl;
        }
    }
    return 0;
}

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

static AVFrame opencv_frame(AVFrame *frame, AVCodecContext *pCodecCtx ) {
    int ret;

    Mat mat = avframe_to_cvmat(frame, pCodecCtx);
//    Mat small = mat(Rect(600,300,300,300));
//    small.copyTo(mat(Rect(50,50,300,300)));

    return cvmat_to_avframe(&mat);
}

static AVFrame cvmat_to_avframe(cv::Mat* frame)
{

    AVFrame dst;

    cv::Size frameSize = frame->size();
    AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_RAWVIDEO);
    AVFormatContext* outContainer = avformat_alloc_context();
    AVStream *outStream = avformat_new_stream(outContainer, encoder);
    avcodec_get_context_defaults3(outStream->codec, encoder);

    outStream->codec->pix_fmt = AV_PIX_FMT_BGR24;
    outStream->codec->width = frame->cols;
    outStream->codec->height = frame->rows;
    avpicture_fill((AVPicture*)&dst, frame->data, AV_PIX_FMT_BGR24, outStream->codec->width, outStream->codec->height);
    dst.width = frameSize.width;
    dst.height = frameSize.height;

    return dst;
}


static cv::Mat avframe_to_cvmat(AVFrame *frame, AVCodecContext *pCodecCtx)
{

    AVFrame dst;
    cv::Mat m;

    memset(&dst, 0, sizeof(dst));

    int w = frame->width, h = frame->height;

    int size = avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
    uint8_t  *out_bufferRGB = (uint8_t *)av_malloc(size);

    m = cv::Mat(h, w, CV_8UC3, out_bufferRGB, Mat::AUTO_STEP);
    dst.data[0] = (uint8_t *)m.data;
    avpicture_fill( (AVPicture *)&dst, dst.data[0], AV_PIX_FMT_BGR24, w, h);

    struct SwsContext *convert_ctx=NULL;
    enum AVPixelFormat src_pixfmt = AV_PIX_FMT_BGR24;
    enum AVPixelFormat dst_pixfmt = AV_PIX_FMT_BGR24;
    convert_ctx = sws_getContext(w, h, src_pixfmt, w, h, dst_pixfmt,
                                 SWS_FAST_BILINEAR, NULL, NULL, NULL);

    sws_scale(convert_ctx, frame->data, frame->linesize, 0, h,
              dst.data, dst.linesize);
    sws_freeContext(convert_ctx);

    return m;
}