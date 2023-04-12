#include <stdint.h>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../nvupscale/filter.h"
#include "../nvvfx/include/nvCVStatus.h"
#include "../nvvfx/include/nvCVImage.h"
#include "../nvvfx/include/nvVideoEffects.h"

char* g_nvVFXSDKPath = nullptr;

#define RETURN_IF_ERROR(e) if((e) != NVCV_SUCCESS) { return err; }

class Processor {
public:
    ~Processor();
    const PIXEL* process(int in_x, int in_y, int out_x, int out_y, const void* in, int ar_mode, int sr_mode);

private:
    NvVFX_Handle ar_{};
    NvVFX_Handle sr_{};
    
    NvCVImage src_wrap_{};
    NvCVImage dst_wrap_{};
    NvCVImage src_gpu_{};
    NvCVImage ar_out_{};
    NvCVImage sr_out_{};
    NvCVImage tmp_buf_{};

    std::vector<PIXEL> dst_buf_{};

    int prev_ar_in_x_ = 0;
    int prev_ar_in_y_ = 0;
    int prev_sr_in_x_ = 0;
    int prev_sr_in_y_ = 0;
    const void* prev_sr_in_pix_ = nullptr;
    int prev_ar_mode_ = -1;
    int prev_sr_mode_ = -1;

    NvCV_Status alloc_buffer(NvCVImage* image, int width, int height) {
        if (image->pixels) {
            return NvCVImage_Alloc(image, width, height, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1);
        }
        else {
            return NvCVImage_Realloc(image, width, height, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1);
        }
    }

    NvCV_Status wrap_image(NvCVImage* image, int width, int height, void* pixels) {
        *image = {};
        image->width = width;
        image->height = height;
        image->pitch = 3 * width;
        image->pixelFormat = NVCV_BGR;
        image->componentType = NVCV_U8;
        image->pixelBytes = 3;
        image->componentBytes = 1;
        image->numComponents = 3;
        image->planar = NVCV_CHUNKY;
        image->gpuMem = NVCV_CPU;
        image->pixels = pixels;

        return NVCV_SUCCESS;
    }

    NvCV_Status process_ar(NvCVImage& in, int mode) {
        NvCV_Status err = {};
        bool reload = false;

        if (!ar_) {
            RETURN_IF_ERROR(err = NvVFX_CreateEffect(NVVFX_FX_ARTIFACT_REDUCTION, &ar_));
            reload = true;
        }

        if (in.width != prev_ar_in_x_ || in.height != prev_ar_in_y_) {
            RETURN_IF_ERROR(err = NvVFX_SetImage(ar_, NVVFX_INPUT_IMAGE, &in));
            prev_ar_in_x_ = in.width;
            prev_ar_in_y_ = in.height;
            reload = true;
        }

        if (!ar_out_.pixels || in.width != ar_out_.width || in.height != ar_out_.height) {
            RETURN_IF_ERROR(err = alloc_buffer(&ar_out_, in.width, in.height));
            RETURN_IF_ERROR(err = NvVFX_SetImage(ar_, NVVFX_OUTPUT_IMAGE, &ar_out_));
            reload = true;
        }

        if (mode != prev_ar_mode_) {
            RETURN_IF_ERROR(err = NvVFX_SetU32(ar_, NVVFX_MODE, mode));
            prev_ar_mode_ = mode;
            reload = true;
        }

        if (reload) {
            RETURN_IF_ERROR(err = NvVFX_Load(ar_));
        }

        RETURN_IF_ERROR(err = NvVFX_Run(ar_, 0));

        return NVCV_SUCCESS;
    }

    bool process_sr(NvCVImage& in, int out_x, int out_y, int mode) {
        NvCV_Status err = {};
        bool reload = false;

        if (!sr_) {
            RETURN_IF_ERROR(err = NvVFX_CreateEffect(NVVFX_FX_SUPER_RES, &sr_));
            reload = true;
        }

        if (in.width != prev_sr_in_x_ || in.height != prev_sr_in_y_ || in.pixels != prev_sr_in_pix_) {
            RETURN_IF_ERROR(err = NvVFX_SetImage(sr_, NVVFX_INPUT_IMAGE, &in));
            prev_sr_in_x_ = in.width;
            prev_sr_in_y_ = in.height;
            prev_sr_in_pix_ = in.pixels;
            reload = true;
        }

        if (!sr_out_.pixels || sr_out_.width != out_x || sr_out_.height != out_y) {
            RETURN_IF_ERROR(err = alloc_buffer(&sr_out_, out_x, out_y));
            RETURN_IF_ERROR(err = NvVFX_SetImage(sr_, NVVFX_OUTPUT_IMAGE, &sr_out_));
            reload = true;
        }

        if (mode != prev_sr_mode_) {
            RETURN_IF_ERROR(NvVFX_SetU32(sr_, NVVFX_MODE, mode));
            prev_sr_mode_ = mode;
            reload = true;
        }

        if (reload) {
            RETURN_IF_ERROR(err = NvVFX_Load(sr_));
        }

        RETURN_IF_ERROR(err = NvVFX_Run(sr_, 0));

        return NVCV_SUCCESS;
    }
};

Processor::~Processor() {
    if (ar_) {
        NvVFX_DestroyEffect(ar_);
    }

    if (sr_) {
        NvVFX_DestroyEffect(sr_);
    }
}

const PIXEL* Processor::process(int in_x, int in_y, int out_x, int out_y, const void* in, int ar_mode, int sr_mode) {
    const bool ar = ar_mode >= 0;
    const bool sr = sr_mode >= 0;

    if (wrap_image(&src_wrap_, in_x, in_y, const_cast<void*>(in)) != NVCV_SUCCESS) {
        return nullptr;
    }

    if (!src_gpu_.pixels || src_gpu_.width != in_x || src_gpu_.height != in_y) {
        if (alloc_buffer(&src_gpu_, in_x, in_y) != NVCV_SUCCESS) {
            return nullptr;
        }
    }

    if (!tmp_buf_.pixels || tmp_buf_.width < out_x || tmp_buf_.height < out_y) {
        if (alloc_buffer(&tmp_buf_, out_x, out_y) != NVCV_SUCCESS) {
            return nullptr;
        }
    }

    if (NvCVImage_Transfer(&src_wrap_, &src_gpu_, 1.f / 255.f, nullptr, nullptr) != NVCV_SUCCESS) {
        return nullptr;
    }

    if (ar) {
        if (process_ar(src_gpu_, ar_mode) != NVCV_SUCCESS) {
            return nullptr;
        }
    }

    if (sr) {
        if (process_sr(ar ? ar_out_ : src_gpu_, out_x, out_y, sr_mode) != NVCV_SUCCESS) {
            return nullptr;
        }
    }

    dst_buf_.resize((size_t)out_x * out_y);
    if (wrap_image(&dst_wrap_, out_x, out_y, dst_buf_.data()) != NVCV_SUCCESS) {
        return nullptr;
    }

    if (NvCVImage_Transfer(sr ? &sr_out_ : &ar_out_, &dst_wrap_, 255.f, nullptr, nullptr) != NVCV_SUCCESS) {
        return nullptr;
    }

    return dst_buf_.data();
}

struct Message {
    int32_t in_x;
    int32_t in_y;
    int32_t out_x;
    int32_t out_y;
    int32_t ar_mode;
    int32_t sr_mode;
    PIXEL pix[4096 * 4096];
};

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE pipe = CreateNamedPipe(
        "\\\\.\\pipe\\nvupscale",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        0,
        0,
        0,
        nullptr
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        return -1;
    }

    if (!ConnectNamedPipe(pipe, nullptr)) {
        CloseHandle(pipe);
        return -1;
    }

    Processor processor{};
    static Message message{};
    DWORD iobyte = 0;

    while (true) {
        if (!ReadFile(pipe, &message, sizeof(message), &iobyte, nullptr) || iobyte != 24 + 3 * (message.in_x * message.in_y)) {
            break;
        }

        auto out = processor.process(message.in_x, message.in_y, message.out_x, message.out_y, message.pix, message.ar_mode, message.sr_mode);
        if (!out) {
            break;
        }

        auto size = 3 * message.out_x * message.out_y;
        if (!WriteFile(pipe, out, size, &iobyte, nullptr) || iobyte != size) {
            break;
        }
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);

	return 0;
}