#ifndef _MDR_GREEDY_BASED_SIZE_INTERPRETER_HPP
#define _MDR_GREEDY_BASED_SIZE_INTERPRETER_HPP

#include "SizeInterpreterInterface.hpp"
#include <queue>
#include "RefactorUtils.hpp"

// inorder and round-robin size interpreter

namespace MDR {
    struct UnitErrorGain{
        double unit_error_gain;
        int level;
        UnitErrorGain(double u, int l) : unit_error_gain(u), level(l) {}
    };
    struct CompareUniteErrorGain{
        bool operator()(const UnitErrorGain& u1, const UnitErrorGain& u2){
            return u1.unit_error_gain < u2.unit_error_gain;
        }
    };
    // greedy bit-plane retrieval
    template<class ErrorEstimator>
    class GreedyBasedSizeInterpreter : public concepts::SizeInterpreterInterface {
    public:
        GreedyBasedSizeInterpreter(const ErrorEstimator& e){
            error_estimator = e;
        }
        std::vector<uint32_t> interpret_retrieve_size(const std::vector<std::vector<uint32_t>>& level_sizes, const std::vector<std::vector<double>>& level_errors, double tolerance, std::vector<uint8_t>& index) const {
            const int num_levels = level_sizes.size();
            std::vector<uint32_t> retrieve_sizes(num_levels, 0);

            double accumulated_error = 0;
            for(int i=0; i<num_levels; i++){
                accumulated_error += error_estimator.estimate_error(level_errors[i][index[i]], i);
            }
            std::priority_queue<UnitErrorGain, std::vector<UnitErrorGain>, CompareUniteErrorGain> heap;
            for(int i=0; i<num_levels; i++){
                double error_gain = error_estimator.estimate_error_gain(accumulated_error, level_errors[i][index[i]], level_errors[i][index[i] + 1], i);
                heap.push(UnitErrorGain(error_gain / level_sizes[i][index[i]], i));
            }            

            bool tolerance_met = false;
            while((!tolerance_met) && (!heap.empty())){
                auto unit_error_gain = heap.top();
                heap.pop();
                int i = unit_error_gain.level;
                int j = index[i];
                retrieve_sizes[i] += level_sizes[i][j];
                accumulated_error -= error_estimator.estimate_error(level_errors[i][j], i);
                accumulated_error += error_estimator.estimate_error(level_errors[i][j + 1], i);
                if(accumulated_error < tolerance){
                    tolerance_met = true;
                }
                index[i] ++;
                if(index[i] != level_sizes[i].size()){
                    double error_gain = error_estimator.estimate_error_gain(accumulated_error, level_errors[i][index[i]], level_errors[i][index[i] + 1], i);
                    heap.push(UnitErrorGain(error_gain / level_sizes[i][index[i]], i));
                }
                std::cout << i;
            }
            std::cout << std::endl;
            std::cout << "Requested tolerance = " << tolerance << ", estimated error = " << accumulated_error << std::endl;
            return retrieve_sizes;
        }
        void print() const {
            std::cout << "Greedy based size interpreter." << std::endl;
        }
    private:
        ErrorEstimator error_estimator;
    };
    // greedy bit-plane retrieval with sign exculsion (excluding the first component)
    template<class ErrorEstimator>
    class SignExcludeGreedyBasedSizeInterpreter : public concepts::SizeInterpreterInterface {
    public:
        SignExcludeGreedyBasedSizeInterpreter(const ErrorEstimator& e){
            error_estimator = e;
        }
        std::vector<uint32_t> interpret_retrieve_size(const std::vector<std::vector<uint32_t>>& level_sizes, const std::vector<std::vector<double>>& level_errors, double tolerance, std::vector<uint8_t>& index) const {
            int num_levels = level_sizes.size();
            std::vector<uint32_t> retrieve_sizes(num_levels, 0);
            double accumulated_error = 0;
            for(int i=0; i<num_levels; i++){
                accumulated_error += error_estimator.estimate_error(level_errors[i][index[i]], i);
            }
            std::priority_queue<UnitErrorGain, std::vector<UnitErrorGain>, CompareUniteErrorGain> heap;
            // identify minimal level
            double min_error = accumulated_error;
            for(int i=0; i<num_levels; i++){
                min_error -= error_estimator.estimate_error(level_errors[i][index[i]], i);
                min_error += error_estimator.estimate_error(level_errors[i][index[i]], i);
                // fetch the first component
                retrieve_sizes[i] += level_sizes[i][index[i]];
                accumulated_error -= error_estimator.estimate_error(level_errors[i][index[i]], i);
                accumulated_error += error_estimator.estimate_error(level_errors[i][index[i] + 1], i);
                //std::cout << i << ", " << +index[i] << std::endl;
                index[i] ++;
                // push the next one
                if(index[i] != level_sizes[i].size()){
                    double error_gain = error_estimator.estimate_error_gain(accumulated_error, level_errors[i][index[i]], level_errors[i][index[i] + 1], i);
                    heap.push(UnitErrorGain(error_gain / level_sizes[i][index[i]], i));
                }
                //std::cout << i;
                //std::cout << i << ", " << retrieve_sizes[i] << std::endl;
                if(min_error < tolerance){
                    // the min error of first 0~i levels meets the tolerance
                    num_levels = i + 1;
                    break;
                }
            }

            bool tolerance_met = false;
            while((!tolerance_met) && (!heap.empty())){
                auto unit_error_gain = heap.top();
                heap.pop();
                int i = unit_error_gain.level;
                int j = index[i];
                retrieve_sizes[i] += level_sizes[i][j];
                accumulated_error -= error_estimator.estimate_error(level_errors[i][j], i);
                accumulated_error += error_estimator.estimate_error(level_errors[i][j + 1], i);
                if(accumulated_error < tolerance){
                    tolerance_met = true;
                }
                //std::cout << i << ", " << +index[i] << std::endl;
                index[i] ++;
                if(index[i] != level_sizes[i].size()){
                    double error_gain = error_estimator.estimate_error_gain(accumulated_error, level_errors[i][index[i]], level_errors[i][index[i] + 1], i);
                    heap.push(UnitErrorGain(error_gain / level_sizes[i][index[i]], i));
                }
                //std::cout << i;
                //std::cout << i << ", " << retrieve_sizes[i] << std::endl;
            }
            //std::cout << std::endl;
            std::cout << "Requested tolerance = " << tolerance << ", estimated error = " << accumulated_error << std::endl;
            return retrieve_sizes;
        }
        void print() const {
            std::cout << "Greedy based size interpreter." << std::endl;
        }
    private:
        ErrorEstimator error_estimator;
    };
}
#endif