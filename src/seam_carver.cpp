#include "seamcarve/seam_carver.hpp"
#include "seamcarve/seam_finders.hpp"
#include <stdexcept>
#include <opencv2/core.hpp>
#include <cstring>


namespace seamcarve {

    namespace {
        Seam FindSeam(const cv::Mat&energy, Algorithm algorithm){
            switch(algorithm){
                case Algorithm::DYNAMIC_PROGRAMMING:
                    return FindSeamDP(energy);
                case Algorithm::GREEDY:
                    return FindSeamGreedy(energy);
                case Algorithm::GRAPH_CUT:
                    return FindSeamGraphCut(energy);
            }
            throw std::invalid_argument("Carve: unknown algorithm");
        }

        cv::Mat CarveWidth(const cv::Mat& image, int targetWidth, const CarveOptions& options){
            cv::Mat current = image.clone(); // Make a copy of the input image to avoid modifying it directly
            while(current.cols > targetWidth){
                const cv::Mat energy = ComputeEnergy(current, options.energyMethod, options.divideAndConquer, options.baseSize);
                const Seam seam = FindSeam(energy, options.algorithm);
                current = RemoveSeam(current, seam, Orientation::VERTICAL);
            }
            return current;
        }
    } // end of anonymous namespace

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

            const uchar* src =  image.ptr<uchar>(row);
            uchar* dst = out.ptr<uchar>(row);
            std::memcpy(dst, src, cut * px);                                  // left of the seam
            std::memcpy(dst + cut * px, src + (cut + 1) * px,
                        (image.cols - cut - 1) * px);                         // right of the seam
        }
        return out;
    }

    cv::Mat Carve(const cv::Mat& image, int targetWidth, int targetHeight, const CarveOptions& options) {
        if(image.empty()){
            throw std::invalid_argument("Carve: input image is empty");
        }
        if(targetWdith < 1 || targetHeight < 1){
            throw std::invalid_argument("Carve: target dimensions must be at least 1x1");
        }
        if(targetWidth > image.cols || targetHeight > image.rows){
            throw std::invalid_argument("Carve: target is larger than the image (only shrinking is supported)");
        }
        cv::Mat result = CarveWidth(image, targetWidth, options);  // Width: vertical seams

        if(targetHeight < result.rows){
            cv::Mat transposed;
            cv::transpose(result, transposed);
            result = CarveWidth(transposed, targetHeight, options);  // Height: horizontal seams
            cv::transpose(result, result);
        }
        return result;
    }
}  // namespace seamcarve
