#include "seamcarve/seam_carver.hpp"
#include "seamcarve/seam_finders.hpp"
#include <stdexcept>
#include <opencv2/core.hpp>
#include <cstring>


namespace seamcarve {

    namespace {
        /**
        * @brief Calls the seam finder chosen in the options.
        * @param[in] energy     - Energy map (CV_64F).
        * @param[in] algorithm  - Which seam finder to use.
        * @return The minimum vertical seam, one column index per row.
        * @throws std::invalid_argument If the algorithm is not recognised.
        */
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
        /**
        * @brief Removes vertical seams one at a time until the image is targetWidth wide.
        * The energy is recomputed after every seam, because removing one changes the pixels next to it.
        * @param[in] image      - 8-bit BGR image (CV_8UC3).
        * @param[in] targetWidth        - Width to stop at, not larger than image.cols.
        * @param[in] options    - Algorithm and energy method.
        * @return A new image targetWidth pixels wide. The input is not modified.
        */
        cv::Mat CarveWidth(const cv::Mat& image, int targetWidth, const CarveOptions& options){
            cv::Mat current = image.clone(); // Make a copy of the input image to avoid modifying it directly
            while(current.cols > targetWidth){
                const cv::Mat energy = ComputeEnergy(current, options.energyMethod, options.divideAndConquerType, options.baseSize);
                const Seam seam = FindSeam(energy, options.algorithm);
                current = RemoveSeam(current, seam, Orientation::VERTICAL);
            }
            return current;
        }
    } // end of anonymous namespace
    /**
    * @brief Removes one seam from the image.
    * A horizontal seam is removed by transposing the image, removing it as a vertical seam, then transposing back.
    * Task 1c.
    * @param[in] image  - Image to remove the seam from.
    * @param[in] seam   - Seam to remove: one index per row (vertical) or per column (horizontal).
    * @param[in] orientation    - Whether the seam is vertical or horizontal.
    * @return A copy of the image, one pixel narrower (vertical) or shorter (horizontal).
    * @throws std::invalid_argument If the image is empty, too small to remove a seam from, or the seam length is wrong.
    * @throws std::out_of_range If a seam index is outside the image.
    */
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
    /**
    * @brief Repeatedly finds and removes the minimum seam until the target size is reached.
    * The width is reduced first with vertical seams. The height is then reduced by transposing the image,
    * carving it the same way and transposing it back, so the seam finders only handle vertical seams.
    * Only shrinking is required by the assignment.
    * @param[in] image  - Input image. It is not modified.
    * @param[in] targetWidth    - Target width in pixels.
    * @param[in] targetHeight   - Target height in pixels.
    * @param[in] options        - Algorithm, energy method and verbosity.
    * @return The resized image.
    * @throws std::invalid_argument If the image is empty, or the target is smaller than 1x1 or larger than the image.
    */
    cv::Mat Carve(const cv::Mat& image, int targetWidth, int targetHeight, const CarveOptions& options) {
        if(image.empty()){
            throw std::invalid_argument("Carve: input image is empty");
        }
        if(targetWidth < 1 || targetHeight < 1){
            throw std::invalid_argument("Carve: target dimensions must be at least 1x1");
        }
        if(targetWidth > image.cols || targetHeight > image.rows){
            throw std::invalid_argument("Carve: target is larger than the image (only shrinking is supported)");
        }
        cv::Mat result = CarveWidth(image, targetWidth, options);  // Width: vertical seams

        if(targetHeight < result.rows){
            cv::Mat transposed;
            cv::transpose(result, transposed);
            transposed = CarveWidth(transposed, targetHeight, options);  // Height: horizontal seams
            cv::transpose(transposed, result);
        }
        return result;
    }
}  // namespace seamcarve
