#include "seamcarve/seam_carver.hpp"
#include "seamcarve/seam_finders.hpp"
#include <algorithm>
#include <stdexcept>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
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
        * @brief Removes a vertical seam from a buffer in place: in every row, shifts the pixels right of the seam one step left.
        * Nothing is allocated. The buffer keeps its size; only its first (width - 1) columns are meaningful afterwards,
        * so the caller tracks the current width and works on a view of that many columns.
        * Works for any element type (BGR image, CV_64F energy map) because it moves raw bytes.
        * @param[in,out] buffer	- Image or map whose rows are shifted.
        * @param[in] width	- Number of meaningful columns before the removal.
        * @param[in] seam	- One column per row, each inside [0, width).
        */
        void RemoveSeamInPlace(cv::Mat& buffer, int width, const Seam& seam) {
            const size_t pixelSize{ buffer.elemSize() };  // bytes per pixel: 3 for BGR, 8 for the double energy map
            for (int row{}; row < buffer.rows; ++row) {
                auto* rowPtr{ buffer.ptr<uchar>(row) };
                // memmove, not memcpy: the source and destination ranges overlap.
                std::memmove(rowPtr + seam[row] * pixelSize, rowPtr + (seam[row] + 1) * pixelSize, (width - seam[row] - 1) * pixelSize);
            }
        }

        /**
        * @brief Removes vertical seams one at a time until the image is targetWidth wide.
        * The energy map is computed once with the chosen method and then kept up to date: removing a seam only
        * changes the pixels next to it, so after each seam only that band is recomputed (with the same method)
        * instead of the whole map. The result is identical to recomputing everything after every seam.
        * @param[in] image              - 8-bit BGR image (CV_8UC3).
        * @param[in] targetWidth        - Width to stop at, not larger than image.cols.
        * @param[in] options            - Algorithm, energy method and the optional progress / cancel / step hooks.
        * @param[in] orientation        - Which way the seams run in the image the user sees: VERTICAL for the width pass, HORIZONTAL
        *                                 for the height pass (where `image` is the transposed image). Only used to label the hooks.
        * @return A new image targetWidth pixels wide. The input is not modified.
        * @throws CarveCancelled If options.cancel is set while carving.
        */
        cv::Mat CarveWidth(const cv::Mat& image, int targetWidth, const CarveOptions& options, Orientation orientation){
            cv::Mat colour{ image.clone() };  // working copy so the input is not modified; it keeps its size, only `width` shrinks
            cv::Mat energy{ ComputeEnergy(colour, options.energyMethod, options.divideAndConquerType, options.baseSize) };  // the only full computation

            int width{ image.cols };  // current number of meaningful columns in colour and energy
            while(width > targetWidth){
                if (options.cancel && options.cancel->load()) throw CarveCancelled{};  // GUI Cancel button

                // The seam finders only see the first `width` columns (a view, no copy); the rest is leftover from earlier shifts.
                const Seam seam{ FindSeam(energy(cv::Rect(0, 0, width, energy.rows)), options.algorithm) };

                // Step-by-step hooks (empty unless the caller wants to watch): the seam is known, the image still has it.
                if (options.onSeamFound) options.onSeamFound(colour(cv::Rect(0, 0, width, colour.rows)), seam, orientation);
                if (options.seamLog) options.seamLog->push_back({ orientation, seam });

                RemoveSeamInPlace(colour, width, seam);  // remove the seam from the image and from the energy map
                RemoveSeamInPlace(energy, width, seam);
                --width;

                // Fix the energy values that changed because the seam's neighbours moved next to each other.
                cv::Mat energyView{ energy(cv::Rect(0, 0, width, energy.rows)) };
                UpdateEnergyAfterSeamRemoval(energyView, colour(cv::Rect(0, 0, width, colour.rows)), seam, options.energyMethod, options.divideAndConquerType, options.baseSize);
                
                if (options.seamsDone) ++*options.seamsDone;  // GUI progress bar
            }
            return colour(cv::Rect(0, 0, width, colour.rows)).clone();  // clone: return a compact image, not a view into the wider buffer
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
        const size_t px{ image.elemSize() };  // bytes per pixel (3 for CV_8UC3)

        for (int row{}; row < image.rows; ++row) {
            const int cut{ seam[row] };

            if (cut < 0 || cut >= image.cols)
                throw std::out_of_range("RemoveSeam: seam index out of range");

            const uchar* src{ image.ptr<uchar>(row) };
            uchar* dst{ out.ptr<uchar>(row) };
            std::memcpy(dst, src, cut * px);                                                // left of the seam
            std::memcpy(dst + cut * px, src + (cut + 1) * px, (image.cols - cut - 1) * px); // right of the seam
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
        cv::Mat result = CarveWidth(image, targetWidth, options, Orientation::VERTICAL);  // Width: vertical seams

        if(targetHeight < result.rows){
            cv::Mat transposed;
            cv::transpose(result, transposed);
            transposed = CarveWidth(transposed, targetHeight, options, Orientation::HORIZONTAL);  // Height: horizontal seams
            cv::transpose(transposed, result);
        }
        return result;
    }

    void PaintSeam(cv::Mat& image, const Seam& seam, Orientation orientation, const cv::Scalar& colour, int thickness) {
        if (image.empty() || image.type() != CV_8UC3)
            throw std::invalid_argument("PaintSeam: image must be a non-empty 8-bit BGR image");

        const bool vertical{ orientation == Orientation::VERTICAL };
        const int lines{ vertical ? image.rows : image.cols };   // a vertical seam has one entry per row, a horizontal one per column
        const int length{ vertical ? image.cols : image.rows };  // how far each entry may point
        if (static_cast<int>(seam.size()) != lines)
            throw std::invalid_argument("PaintSeam: seam length does not match the image");

        const cv::Vec3b paint{ cv::saturate_cast<uchar>(colour[0]), cv::saturate_cast<uchar>(colour[1]), cv::saturate_cast<uchar>(colour[2]) };
        for (int line{}; line < lines; ++line) {
            if (seam[line] < 0 || seam[line] >= length)
                throw std::out_of_range("PaintSeam: seam index out of range");

            const int last{ std::min(seam[line] + std::max(thickness, 1), length) };  // extra width goes right of / below the seam
            for (int position{ seam[line] }; position < last; ++position) {
                if (vertical) image.at<cv::Vec3b>(line, position) = paint;
                else          image.at<cv::Vec3b>(position, line) = paint;
            }
        }
    }

    cv::Mat BuildRemovalMap(cv::Size originalSize, const std::vector<SeamStep>& steps) {
        if (originalSize.width < 1 || originalSize.height < 1)
            throw std::invalid_argument("BuildRemovalMap: the original size must be at least 1x1");

        cv::Mat removedAt(originalSize, CV_32S, cv::Scalar(-1));

        // origin(row, col) = where the pixel that is now at (row, col) was in the original image, stored as (x, y).
        // It is shrunk by the same seams as the image, so it always says where each remaining pixel came from.
        cv::Mat origin(originalSize, CV_32SC2);
        for (int y{}; y < origin.rows; ++y) {
            for (int x{}; x < origin.cols; ++x) origin.at<cv::Vec2i>(y, x) = cv::Vec2i(x, y);
        }

        // Like Carve(), horizontal seams are handled by transposing: while the steps are horizontal, `origin` is
        // the transposed image, where a horizontal seam is an ordinary vertical one.
        Orientation current{ Orientation::VERTICAL };
        int width{ origin.cols };  // meaningful columns of `origin` (it keeps its size while seams are removed in place)

        for (int index{}; index < static_cast<int>(steps.size()); ++index) {
            const SeamStep& step{ steps[index] };
            if (step.orientation != current) {
                cv::Mat transposed;
                cv::transpose(origin(cv::Rect(0, 0, width, origin.rows)), transposed);
                origin = transposed;
                width = origin.cols;
                current = step.orientation;
            }

            if (width < 2 || static_cast<int>(step.seam.size()) != origin.rows)
                throw std::invalid_argument("BuildRemovalMap: a seam does not fit the image it should be removed from");

            for (int line{}; line < origin.rows; ++line) {
                const int position{ step.seam[line] };
                if (position < 0 || position >= width)
                    throw std::out_of_range("BuildRemovalMap: seam index out of range");

                const cv::Vec2i where{ origin.at<cv::Vec2i>(line, position) };  // this pixel's place in the original image
                removedAt.at<int>(where[1], where[0]) = index;
            }
            RemoveSeamInPlace(origin, width, step.seam);
            --width;
        }
        return removedAt;
    }

    cv::Mat RenderSeamOverlay(const cv::Mat& original, const cv::Mat& removalMap, int stepCount) {
        if (original.empty() || original.type() != CV_8UC3 || removalMap.type() != CV_32S || removalMap.size() != original.size())
            throw std::invalid_argument("RenderSeamOverlay: needs a CV_8UC3 image and a CV_32S removal map of the same size");

        cv::Mat overlay{ original.clone() };
        if (stepCount < 1) return overlay;

        // 256 colours from blue (first seam) to red (last seam).
        cv::Mat ramp(256, 1, CV_8U), colours;
        for (int i{}; i < 256; ++i) ramp.at<uchar>(i) = static_cast<uchar>(i);
        cv::applyColorMap(ramp, colours, cv::COLORMAP_TURBO);

        for (int y{}; y < overlay.rows; ++y) {
            const int* removedRow{ removalMap.ptr<int>(y) };
            auto* overlayRow{ overlay.ptr<cv::Vec3b>(y) };
            for (int x{}; x < overlay.cols; ++x) {
                if (removedRow[x] < 0) continue;  // still in the result: keep the original colour
                const int shade{ stepCount > 1 ? std::clamp(removedRow[x] * 255 / (stepCount - 1), 0, 255) : 0 };
                overlayRow[x] = colours.at<cv::Vec3b>(shade);
            }
        }
        return overlay;
    }

    cv::Mat RenderCarveStep(const cv::Mat& original, const std::vector<SeamStep>& steps, int stepIndex, int seamThickness) {
        if (stepIndex < 0 || stepIndex > static_cast<int>(steps.size()))
            throw std::out_of_range("RenderCarveStep: stepIndex must be between 0 and the number of steps");

        cv::Mat frame{ original.clone() };
        for (int index{}; index < stepIndex; ++index)
            frame = RemoveSeam(frame, steps[index].seam, steps[index].orientation);

        if (stepIndex < static_cast<int>(steps.size()))
            PaintSeam(frame, steps[stepIndex].seam, steps[stepIndex].orientation, cv::Scalar(0, 0, 255), seamThickness);
        return frame;
    }
}  // namespace seamcarve
