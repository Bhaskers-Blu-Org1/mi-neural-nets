/*!
 * Copyright (C) tkornuta, IBM Corporation 2015-2019
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*!
 * @file ConvHebbian.hpp
 * @brief
 * @Author: Alexis Asseman <alexis.asseman@ibm.com>, Tomasz Kornuta <tkornut@us.ibm.com>
 * @Date:   May 30, 2017
 *
 * Copyright (c) 2017, Alexis Asseman, Tomasz Kornuta, IBM Corporation. All rights reserved.
 *
 */

#ifndef SRC_MLNN_CONVHEBBIAN_HPP_
#define SRC_MLNN_CONVHEBBIAN_HPP_

#include <mlnn/layer/Layer.hpp>

namespace mic {
namespace mlnn {
namespace experimental {

/*!
 * \brief Class implementing a convolutional hebbian layer.
 * \tparam eT Template parameter denoting precision of variables (float for calculations/double for testing).
 */
template <typename eT=float>
class ConvHebbian : public mic::mlnn::Layer<eT> {
public:

    /*!
     * Creates the convolutional hebbian layer.
     * @param inputs_ Length of the input vector.
     * @param outputs_ Length of the output vector.
     * @param name_ Name of the layer.
     */
    ConvHebbian<eT>(size_t input_width, size_t input_height, size_t input_depth, size_t nfilters, size_t filter_size, size_t stride = 1, std::string name_ = "ConvHebbian") :
        Layer<eT>(input_height, input_width, input_depth, (input_height - filter_size) / stride, (input_width - filter_size) / stride, 1, LayerTypes::ConvHebbian, name_),
        nfilters(nfilters),
        filter_size(filter_size),
        stride(stride),
        x2col(new mic::types::Matrix<eT>(filter_size * filter_size, output_width * output_height)),
        conv2col(new mic::types::Matrix<eT>(filter_size * filter_size, output_width * output_height))
    {
        // Create the weights matrix, each row is a filter kernel
        p.add("W", nfilters, filter_size * filter_size);
        mic::types::MatrixPtr<eT> W = p["W"];

        // Set normalized, zero sum, hebbian learning as default optimization function.
        Layer<eT>::template setOptimization<mic::neural_nets::learning::NormalizedZerosumHebbianRule<eT> > ();

        // Initialize weights of all the columns of W.
        W->rand();
        for(auto i = 0 ; i < W->rows() ; i++) {
            // Make the matrix Zero Sum
            W->row(i).array() -= W->row(i).sum() / W->row(i).cols();
            if(W->row(i).norm() != 0){
                W->row(i) = W->row(i).normalized();
            }
        }
    }


    /*!
     * Virtual destructor - empty.
     */
    virtual ~ConvHebbian() {}

    /*!
     * Forward pass.
     * @param test_ It is set to true in test mode (network verification).
     */
    void forward(bool test_ = false) {
        // Get input matrices.
        mic::types::Matrix<eT> x = (*s["x"]);
        mic::types::Matrix<eT> W = (*p["W"]);
        // Get output pointer - so the results will be stored!
        mic::types::MatrixPtr<eT> y = s["y"];

        // IM2COL
        // Iterate over the output matrix (number of image patches)
        for(size_t oy = 0 ; oy < output_height ; oy++){
            for(size_t ox = 0 ; ox < output_width ; ox++){
                // Iterate over the rows of the patch
                for(size_t patch_y = 0 ; patch_y < filter_size ; patch_y++){
                    // Copy each row of the image patch into appropriate position in x2col
                    x2col->block(patch_y * filter_size, ox + (output_width * oy), filter_size, 1) =
                            x.block((((oy * stride) + patch_y) * input_width) + (ox * stride), 0, filter_size, 1);
                }
            }
        }
        // Forward pass.
        (*y) = W * (*x2col);
        o_reconstruction_updated = false;
        // ReLU
        //(*y) = (*y).cwiseMax(0);
    }

    /*!
     * Backward pass.
     */
    void backward() {
        //LOG(LERROR) << "Backward propagation should not be used with layers using Hebbian learning!";
    }

    /*!
     * Applies the gradient update, using the selected hebbian rule.
     * @param alpha_ Learning rate - passed to the optimization functions of all layers.
     * @param decay_ Weight decay rate (determining that the "unused/unupdated" weights will decay to 0) (DEFAULT=0.0 - no decay).
     */
    void update(eT alpha_, eT decay_  = 0.0f) {
        opt["W"]->update(p["W"], x2col, s["y"], alpha_);
    }



    /*!
     * Returns activations of weights.
     */
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > & getOutputActivations() {

        // Allocate memory.
        lazyAllocateMatrixVector(o_activations, nfilters, output_height * output_width, 1);

        mic::types::MatrixPtr<eT> W = s["y"];

        // Iterate through "neurons" and generate "activation image" for each one.
        for (size_t i = 0 ; i < nfilters ; i++) {
            // Get row.
            mic::types::MatrixPtr<eT> row = o_activations[i];
            // Copy data.
            (*row) = W->row(i);
            // Resize row.
            row->resize(output_width, output_height);

        }//: for filters

        // Return activations.
        return o_activations;
    }

    /*!
     * Returns reconstruction from feature maps and filters
     */
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > & getOutputReconstruction() {

        // Allocate memory.
        lazyAllocateMatrixVector(o_reconstruction, 1, input_width, input_height);
        o_reconstruction[0]->zeros();
        conv2col->zeros();

        mic::types::MatrixPtr<eT> o = s["y"];
        mic::types::MatrixPtr<eT> w = p["W"];

        //Reconstruct in im2col format
        for(size_t i = 0 ; i < output_width * output_height ; i++){
            for(size_t ker = 0 ; ker < nfilters ; ker++){
                // ReLU on the filters and feature maps
                mic::types::Matrix<eT> k;
                k = (w->row(ker)).transpose();
                k = k.array().max(0.);
                conv2col->col(i) += ((*o)(ker, i) > 0 ? (*o)(ker, i) : 0) * k;
                // No ReLU at all
                //                conv2col->col(i) += ((*o)(ker, i) > 0 ? (*o)(ker, i) : 0)
                //                        * w->row(ker);
            }
        }

        for(size_t x = 0 ; x < output_width ; x ++){
            for(size_t y = 0 ; y < output_height ; y ++){
                for(size_t ker = 0 ; ker < nfilters ; ker++){
                    mic::types::Matrix<eT> temp;
                    temp = conv2col->col(y + (x * output_height));
                    temp.resize(filter_size, filter_size);
                    o_reconstruction[0]->block(y * stride, x * stride, filter_size, filter_size) += temp;
                }
            }
        }

        // Return reconstruction
        o_reconstruction_updated = true;
        return o_reconstruction;
    }

    /*!
     * \brief getOutputReconstructionError
     * \return Squared reconstruction error
     * \details If the reconstruction hasn't been updated, will call getOutputReconstruction(). To avoid computing the reconstruction twice, remember to call getOutputReconstruction() first if you need it.
     */
    eT getOutputReconstructionError() {
        if(!o_reconstruction_updated) {
            getOutputReconstruction();
        }

        Eigen::Map<Eigen::Matrix<eT, Eigen::Dynamic, 1> > r(o_reconstruction[0]->data(), o_reconstruction[0]->size());

        mic::types::Matrix<eT> diff;
        diff = r.normalized() - (*s["x"]).normalized();
        eT error = diff.squaredNorm();
        return error;
    }

    /*!
     * Returns activations of weights.
     */
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > & getWeightActivations() {

        // Allocate memory.
        lazyAllocateMatrixVector(w_activations, nfilters, filter_size*filter_size, 1);

        mic::types::MatrixPtr<eT> W = p["W"];

        // Iterate through "neurons" and generate "activation image" for each one.
        for (size_t i = 0 ; i < nfilters ; i++) {
            // Get row.
            mic::types::MatrixPtr<eT> row = w_activations[i];
            // Copy data.
            (*row) = W->row(i);
            // Resize row.
            row->resize(filter_size, filter_size);
        }//: for filters

        // Return activations.
        return w_activations;
    }

    /*!
     * \brief Returns cosine similarity matrix of filters.
     * \details Give only positive similarities above the diagonal, and negative ones below, else 0.
     * @param fillDiagonal Fill the diagonal with alternating 1,-1 to 'calibrate' the visualization.
     */
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > & getWeightSimilarity(bool fillDiagonal = false) {

        // Allocate memory.
        lazyAllocateMatrixVector(w_similarity, 1, nfilters * nfilters, 1);

        mic::types::MatrixPtr<eT> W = p["W"];
        mic::types::MatrixPtr<eT> row = w_similarity[0];

        // Iterate through "neurons" and generate "activation image" for each one.
        for (size_t i = 0 ; i < nfilters ; i++) {
            for(size_t j = 0 ; j < i ; j++) {
                // Compute cosine similarity between filter i and j
                eT sim = W->row(j).dot(W->row(i));
                sim /= W->row(i).norm() * W->row(j).norm();
                if(sim > 0.)    // positive similarity above diagonal
                    (*row)(j + (nfilters * i)) = sim;
                else            // negative similarity below diagonal
                    (*row)(i + (nfilters * j)) = sim;
            }
        }

        if(fillDiagonal) {
            // Fill diagonal with alternating 1,-1 to 'calibrate' visualization
            for (size_t i = 0 ; i < nfilters ; i++) {
                (*row)(i + (nfilters * i)) = 1 - (int)(2 * (i % 2));
            }
        }

        row->resize(nfilters, nfilters);

        // Return activations.
        return w_similarity;
    }

    /*!
     * Returns dissimilarity of filters.
     */
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > & getWeightDissimilarity() {

        // Allocate memory.
        lazyAllocateMatrixVector(w_dissimilarity, 1, nfilters * nfilters, 1);

        mic::types::MatrixPtr<eT> W = p["W"];
        mic::types::MatrixPtr<eT> row = w_dissimilarity[0];

        // Iterate through "neurons" and generate "activation image" for each one.
        for (size_t i = 0 ; i < nfilters ; i++) {
            for(size_t j = 0 ; j < nfilters ; j++){
                // Compute cosine similarity between filter i and j
                (*row)(j + (nfilters * i)) = std::abs(W->row(j).dot(W->row(i)));
                (*row)(j + (nfilters * i)) /= W->row(i).norm() * W->row(j).norm();
                // Convert to sine
                (*row)(j + (nfilters * i)) = std::sqrt(1 - std::pow((*row)(j + (nfilters * i)), 2));
            }
        }

        row->resize(nfilters, nfilters);

        // Return activations.
        return w_dissimilarity;
    }

    /*!
      * Returns mean value of dissimilarity.
      */



    // Unhide the overloaded methods inherited from the template class Layer fields via "using" statement.
    using Layer<eT>::forward;
    using Layer<eT>::backward;

protected:
    // Unhide the fields inherited from the template class Layer via "using" statement.
    using Layer<eT>::s;
    using Layer<eT>::m;
    using Layer<eT>::p;
    using Layer<eT>::opt;

    // Uncover "sizes" for visualization.
    using Layer<eT>::input_height;
    using Layer<eT>::input_width;
    using Layer<eT>::input_depth;
    using Layer<eT>::output_height;
    using Layer<eT>::output_width;
    using Layer<eT>::output_depth;
    using Layer<eT>::batch_size;

    // Uncover methods useful in visualization.
    using Layer<eT>::lazyAllocateMatrixVector;

    size_t nfilters = 0;
    size_t filter_size = 0;
    size_t stride = 0;
    // Vector of channels, Each containing a vector of filters
    std::vector<std::vector<mic::types::Matrix<eT> > > W;
    mic::types::MatrixPtr<eT> x2col;
    mic::types::MatrixPtr<eT> conv2col;

private:
    // Friend class - required for using boost serialization.
    template<typename tmp> friend class mic::mlnn::MultiLayerNeuralNetwork;

    /// Vector containing activations of neurons.
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > w_activations;
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > o_activations;
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > o_reconstruction;
    bool o_reconstruction_updated = false;
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > w_similarity;
    std::vector< std::shared_ptr <mic::types::Matrix<eT> > > w_dissimilarity;
    /*!
     * Private constructor, used only during the serialization.
     */
    ConvHebbian<eT>() : Layer<eT> () { }
};


} /* namespace fully_connected */
} /* namespace mlnn */
} /* namespace mic */

#endif /* SRC_MLNN_CONVHEBBIAN_HPP_ */
