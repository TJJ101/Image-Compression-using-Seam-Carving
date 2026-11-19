#include "seamcarve/seam_carver.hpp"
#include <stdexcept>
#include <opencv2/core.hpp>
#include <cstring>

namespace seamcarve {
    cv::Mat RemoveSeam(const cv::Mat& image, const Seam& seam, Orientation orientation) {
        if (orientation == Orientation::HORIZONTAL) {
            cv::Mat transposed, result;
            cv::transpose(image, transposed);
            cv::transpose(RemoveSeam(transposed, seam, Orientation::VERTICAL), result);
            return result;
        }

        if (image.empty() || image.cols < 2){
            throw std::invalid_argument("RemoveSeam: image must have at least 2 columns");
        }
        if (static_cast<int>(seam.size()) != image.rows){
            throw std::invalid_argument("RemoveSeam: seam length must equal image rows");
        }
        cv::Mat out(image.rows, image.cols - 1, image.type());
        const size_t px =  image.elemSize();          // bytes per pixel (3 for CV_8UC3)

        for (int row = 0; row < image.rows; ++row) {
            const int cut = seam[row];

            if (cut < 0 || cut >= image.cols)
                throw std::out_of_range("RemoveSeam: seam index out of range");

            const uchar* sr =  image.ptr<uchar>(row);
            uchar* dst = out.ptr<uchar>(row);
            std::memcpy(dst, src, cut * px);                                  // left of the seam
            std::memcpy(dst + cut * px, src + (cut + 1) * px,
                        (image.cols - cut - 1) * px);                         // right of the seam
        }
        return out;
    }

    cv::Mat Carve(const cv::Mat& /*image*/, int /*targetWidth*/, int /*targetHeight*/, const CarveOptions& /*options*/) {
        // TODO(Task 1c): loop { ComputeEnergy -> find*Seam (by options.algorithm) -> removeSeam }
        // until the target size is reached. Handle horizontal seams by transposing.
        throw std::logic_error("Carve: not implemented");
    }
}  // namespace seamcarve
