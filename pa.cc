#include <math.h>
#include <pulse/pulseaudio.h>
#include <pulse/stream.h>
#include <pulse/context.h>
#include <pulse/mainloop-api.h>

#include <iostream>
#include <unistd.h>


struct Eth0Scope {
    pa_context *ctx;
    pa_stream *stream;
    unsigned long phase;
    Eth0Scope(pa_context *ctx) : ctx{ctx}, stream{nullptr}, phase{0} {}
};

std::ostream &
operator << (std::ostream &os, pa_context_state state) {

#define STATE(val) case val: os << #val; break
    switch (state) {
        STATE(PA_CONTEXT_UNCONNECTED);
        STATE(PA_CONTEXT_CONNECTING);
        STATE(PA_CONTEXT_AUTHORIZING);
        STATE(PA_CONTEXT_SETTING_NAME);
        STATE(PA_CONTEXT_READY);
        STATE(PA_CONTEXT_FAILED);
        STATE(PA_CONTEXT_TERMINATED);
        default: os << "unkown state " << int(state) << std::endl;
    }
#undef STATE
    return os;
}

static void
freedata(void *p)
{
    std::clog << "freeing sample buffer" << p << std::endl;
    uint16_t *samples = reinterpret_cast<uint16_t *>(p);
    delete[] samples;
}

static void
canwrite(pa_stream *s, size_t maxbytes, void *udata)
{
    Eth0Scope *scope = reinterpret_cast<Eth0Scope *>(udata);
    std::clog << "can write " << maxbytes << " to " << s << std::endl;
    size_t count = 0;

    auto nsamp = maxbytes/2;
    short *data = new short[nsamp];
    for (size_t sample = 0; sample < nsamp; ++sample ) {
        auto phase = scope->phase;
        scope->phase = (scope->phase + 1) % 200;
        data[sample] = sin(double(phase) * 3.14159 * 2 / 200) * 16384;
    }
    auto rc = pa_stream_write(s, data, maxbytes, freedata, 0, PA_SEEK_RELATIVE);
    std::clog << "wrote " << maxbytes <<": " << rc << std::endl;
}

static void
ctxcb(pa_context *c, void *udata)
{
    Eth0Scope *scope = reinterpret_cast<Eth0Scope *>(udata);
    auto state = pa_context_get_state(c);
    std::clog << "context state change: " << state << std::endl;
    switch (state) {
        case PA_CONTEXT_READY: {
            assert( scope->stream == nullptr);
            pa_sample_spec ss;
            ss.format = PA_SAMPLE_S16LE;
            ss.rate = 44100;
            ss.channels = 1; // 2 = send/receive?
            scope->stream = pa_stream_new(c, "network-auralization", &ss, nullptr);
            std::clog << "created new stream " << scope->stream << std::endl;
            int rv = pa_stream_connect_playback(scope->stream, nullptr, nullptr, PA_STREAM_NOFLAGS, nullptr, nullptr);
            std::clog << "connecting stream:  " << rv << std::endl;
            pa_stream_set_write_callback(scope->stream, canwrite, scope);
            break;
       }
    }
}

int
main(int argc, char *argv[])
{

    auto mainloop = pa_mainloop_new();
    auto api = pa_mainloop_get_api(mainloop);
    pa_context *ctx = pa_context_new(api, "eth0scope");
    Eth0Scope scope(ctx);
    pa_context_set_state_callback(ctx, ctxcb, &scope);

    int rc = pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    if (rc < 0)  {
        std::cerr << "pa_context_connect failed: " << rc << std::endl;
        return 0;
    }
    for (;;) {
        int rv, retval;
        rv = pa_mainloop_iterate(mainloop, true, &retval);
        if (rv < 0)
            break;
    }
}
