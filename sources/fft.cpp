#include <fft.h>

#include <fftw3.h>
#include <math.h>

class DctCalculator::Impl {
public:
    std::vector<double> *input, *output;
    fftw_plan plan;
    size_t width;
};

DctCalculator::DctCalculator(size_t width, std::vector<double> *input, std::vector<double> *output)
    : impl_() {
    if (input->size() != output->size()) {
        throw std::invalid_argument("different input/output sizes");
    }
    if (input->size() != static_cast<__int128>(width) * width) {  // sorry
        throw std::invalid_argument("input.size() != width * width");
    }

    impl_ = std::make_unique<Impl>();

    impl_->input = input, impl_->output = output;
    impl_->width = width;

    impl_->plan =
        fftw_plan_r2r_2d(impl_->width, impl_->width, impl_->input->data(), impl_->output->data(),
                         FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE);
}

void DctCalculator::Inverse() {
    constexpr double kMagicMultiplier = 1. / 16.;

    for (size_t i = 0, j = 0; i < impl_->width; i++, j += impl_->width) {
        impl_->input->data()[i] *= M_SQRT2;
        impl_->input->data()[j] *= M_SQRT2;
    }
    for (auto &i : *impl_->input) {
        i *= kMagicMultiplier;
    }
    fftw_execute(impl_->plan);
}

DctCalculator::~DctCalculator() {
    fftw_destroy_plan(impl_->plan);
}
