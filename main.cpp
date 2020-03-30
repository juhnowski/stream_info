#include <iostream>
#include <vector>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

}

#undef av_err2str
#define av_err2str(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)

using namespace std;

int width = 0;
int height = 0;
int fps = 30;
int bitrate = 300000;

void initialize_avformat_context(AVFormatContext *&fctx, const char *format_name)
{
    int ret = avformat_alloc_output_context2(&fctx, nullptr, format_name, nullptr);
    if (ret < 0)
    {
        std::cout << "Could not allocate output format context!" << std::endl;
        exit(1);
    }
}

void initialize_io_context(AVFormatContext *&fctx, const char *output)
{
    if (!(fctx->oformat->flags & AVFMT_NOFILE))
    {
        int ret = avio_open2(&fctx->pb, output, AVIO_FLAG_WRITE, nullptr, nullptr);
        if (ret < 0)
        {
            std::cout << "initialize_io_context: Could not open output IO context!" << std::endl;
            exit(1);
        }
    }
}


void set_codec_params(AVFormatContext *&fctx, AVCodecContext *&codec_ctx)
{
    const AVRational dst_fps = {fps, 1};

    codec_ctx->codec_tag = 0;
    codec_ctx->codec_id = AV_CODEC_ID_H264;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->gop_size = 12;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->framerate = dst_fps;
    codec_ctx->time_base = av_inv_q(dst_fps);
    codec_ctx->bit_rate = bitrate;
    if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
}

void initialize_codec_stream(AVStream *&stream, AVCodecContext *&codec_ctx, AVCodec *&codec, std::string codec_profile)
{
    int ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        std::cout << "Could not initialize stream codec parameters!" << std::endl;
        exit(1);
    }

    AVDictionary *codec_options = nullptr;
    av_dict_set(&codec_options, "profile", codec_profile.c_str(), 0);
    av_dict_set(&codec_options, "preset", "superfast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);

    // open video encoder
    ret = avcodec_open2(codec_ctx, codec, &codec_options);
    if (ret < 0)
    {
        std::cout << "Could not open video encoder!" << std::endl;
        exit(1);
    }
}


int main(int argc, char **argv)
{
//    AVFormatContext *input_format_context = NULL, *output_format_context = NULL;
    AVPacket packet;
//    const char *in_filename, *out_filename;
    int ret, i;
    int stream_index = 0;
    int *streams_list = NULL;
    int number_of_streams = 0;
//    int fragmented_mp4_options = 0;
//
//    if (argc < 3) {
//        printf("You need to pass at least two parameters.\n");
//        return -1;
//    } else if (argc == 4) {
//        fragmented_mp4_options = 1;
//    }
//
//    in_filename  = argv[1];
//    //out_filename = argv[2];
//    std::string outputServer = argv[2];
//    std::string h264profile = "high444";
//    const char *output = outputServer.c_str();
//
//
//

//
//    avformat_alloc_output_context2(&output_format_context, nullptr, "flv", nullptr);
//    if (!output_format_context) {
//        fprintf(stderr, "Could not create output context\n");
//        ret = AVERROR_UNKNOWN;
//        return -1;
//    }
//
//    int ret1 = avio_open2(&output_format_context->pb, output, AVIO_FLAG_WRITE, nullptr, nullptr);
//    if (ret1 < 0)
//    {
//        std::cout << "initialize_io_context: Could not open output IO context!" << std::endl;
//        exit(1);
//    }
    std::string h264profile = "high444";
    const char *in_filename;
    in_filename  = argv[1];
    std::string inputSdp = argv[1];
    std::string outputServer = argv[2];

    cout << "Set sdp file: " << inputSdp << endl;
    cout << "Set rtmp url: " << outputServer  << endl;

    av_log_set_level(AV_LOG_DEBUG);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();

    const char *output = outputServer.c_str();


    AVFormatContext *output_format_context = nullptr;
    AVCodec *out_codec = nullptr;
    AVStream *out_stream = nullptr;
    AVCodecContext *out_codec_ctx = nullptr;

    initialize_avformat_context(output_format_context, "flv");
    output_format_context->protocol_whitelist = "file,tcp,rtmp,udp,rtp";
    initialize_io_context(output_format_context, output);

    out_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    out_stream = avformat_new_stream(output_format_context, out_codec);
    out_codec_ctx = avcodec_alloc_context3(out_codec);

    set_codec_params(output_format_context, out_codec_ctx);
    initialize_codec_stream(out_stream, out_codec_ctx, out_codec, h264profile);

    out_stream->codecpar->extradata = out_codec_ctx->extradata;
    out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;

    av_dump_format(output_format_context, 0, output, 1);

//    auto *swsctx = initialize_sample_scaler(out_codec_ctx, width, height);
//    auto *frame = allocate_frame_buffer(out_codec_ctx, width, height);

    int cur_size;
    uint8_t *cur_ptr;

    ret = avformat_write_header(output_format_context, nullptr);
    if (ret < 0)
    {
        std::cout << "Could not write header!" << std::endl;
        exit(1);
    }

    AVFormatContext *input_format_context = NULL;
    AVDictionary *d = NULL;
    av_dict_set(&d, "protocol_whitelist", "file,rtp,udp",0);

    if ((ret = avformat_open_input(&input_format_context, in_filename, NULL, &d)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", in_filename);
        return -1;
    }
    if ((ret = avformat_find_stream_info(input_format_context, NULL)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        return -1;
    }
//----------------------------------------------------------------
    number_of_streams = input_format_context->nb_streams;
    streams_list = (int *)av_mallocz_array(number_of_streams, sizeof(*streams_list));

    if (!streams_list) {
        ret = AVERROR(ENOMEM);
        return -1;
    }

    for (i = 0; i < input_format_context->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = input_format_context->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            streams_list[i] = -1;
            continue;
        }
        streams_list[i] = stream_index++;
        out_stream = avformat_new_stream(output_format_context, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            return -1;
        }
        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            return -1;
        }
    }

    while (1) {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(input_format_context, &packet);
        if (ret < 0)
            break;
        in_stream  = input_format_context->streams[packet.stream_index];
        if (packet.stream_index >= number_of_streams || streams_list[packet.stream_index] < 0) {
            av_packet_unref(&packet);
            continue;
        }
        packet.stream_index = streams_list[packet.stream_index];
        out_stream = output_format_context->streams[packet.stream_index];
        /* copy packet */
        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        // https://ffmpeg.org/doxygen/trunk/structAVPacket.html#ab5793d8195cf4789dfb3913b7a693903
        packet.pos = -1;

        //https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga37352ed2c63493c38219d935e71db6c1
        ret = av_interleaved_write_frame(output_format_context, &packet);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&packet);
    }
    //https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga7f14007e7dc8f481f054b21614dfec13
    av_write_trailer(output_format_context);
    end:
    avformat_close_input(&input_format_context);
    /* close output */
    if (output_format_context && !(output_format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_format_context->pb);
    avformat_free_context(output_format_context);
    av_freep(&streams_list);
    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
