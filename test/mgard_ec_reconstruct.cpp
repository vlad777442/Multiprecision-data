#include <iostream>
#include <ctime>
#include <cstdlib>
#include <vector>
#include <iomanip>
#include <cmath>
#include <bitset>
#include <queue>
#include <numeric>
#include <type_traits>

#include <adios2.h>

#include "utils.hpp"
#include "../include/Decomposer/Decomposer.hpp"
#include "../include/Interleaver/Interleaver.hpp"
#include "../include/BitplaneEncoder/BitplaneEncoder.hpp"
#include "../include/Retriever/Retriever.hpp"
#include "../include/ErrorEstimator/ErrorEstimator.hpp"
#include "../include/ErrorCollector/ErrorCollector.hpp"
#include "../include/SizeInterpreter/SizeInterpreter.hpp"
#include "../include/LosslessCompressor/LevelCompressor.hpp"
#include "../include/RefactorUtils.hpp"


#include <erasurecode.h>
#include <erasurecode_helpers.h>
#include <config_liberasurecode.h>
#include <erasurecode_stdinc.h>
#include <erasurecode_version.h>

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"

using namespace ROCKSDB_NAMESPACE;


std::vector<std::vector<uint32_t>> get_level_sizes(uint32_t levels, const std::vector<std::vector<uint64_t>>& query_table)
{
    std::vector<std::vector<uint32_t>> level_sizes(levels);
    for (size_t i = 0; i < query_table.size(); i++)
    {
        level_sizes[query_table[i][0]].push_back(query_table[i][4]);
    }
    return level_sizes;   
}


void shuffle(std::vector<size_t> &arr, size_t n, unsigned int seed)
{
    if (n > 1) 
    {
        size_t i;
        srand(seed);
        for (i = 0; i < n - 1; i++) 
        {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          size_t t = arr[j];
          arr[j] = arr[i];
          arr[i] = t;
        }
    }
}

std::vector<size_t> randomly_mark_site_as_unavailable(size_t total_site, size_t unavailable_site, unsigned int seed)
{
    std::vector<size_t> site_id_list(total_site);
    std::iota(site_id_list.begin(), site_id_list.end(), 0);
    // for (size_t i = 0; i < dev_id_list.size(); i++)
    // {
    //     std::cout << dev_id_list[i] << " ";
    // }
    // std::cout << std::endl;
    shuffle(site_id_list, total_site, seed);
    // for (size_t i = 0; i < dev_id_list.size(); i++)
    // {
    //     std::cout << dev_id_list[i] << " ";
    // }
    // std::cout << std::endl;
    std::vector<size_t> unavailable_site_list(unavailable_site);
    for (size_t i = 0; i < unavailable_site; i++)
    {
        unavailable_site_list[i] = site_id_list[i];
    }
    
    return unavailable_site_list;
}

template <typename T>
std::string PackSingleElement(const T* data)
{
    std::string d(sizeof(T), L'\0');
    memcpy(&d[0], data, d.size());
    return d;
}

template <typename T>
std::unique_ptr<T> UnpackSingleElement(const std::string& data)
{
    if (data.size() != sizeof(T))
        return nullptr;

    std::unique_ptr<T> d(new T);
    memcpy(d.get(), data.data(), data.size());
    return d;
}

template <typename T>
std::string PackVector(const std::vector<T> data)
{
    std::string d(sizeof(T)*data.size(), L'\0');
    memcpy(&d[0], data.data(), d.size());
    return d;
}

template <typename T>
std::vector<T> UnpackVector(const std::string& data)
{
    int size = data.size()/sizeof(T);
    std::vector<T> d(size);
    memcpy(d.data(), data.data(), data.size());
    return d;
}

int main(int argc, char *argv[])
{
    std::string rocksDBPath;
    std::string variableName;
    int error_mode = 0;
    int totalSites = 0;
    int unavaialbleSites = 0; 
    double mgard_s_param;
    std::string rawDataFileName;
    for (size_t i = 0; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-kvs" || arg == "--kvstore")
        {
            if (i+1 < argc)
            {
                rocksDBPath = argv[i+1];
            }
            else
            {
                std::cerr << "--kvstore option requires one argument." << std::endl;
                return 1;
            }            
        } 
        else if (arg == "-var" || arg == "--variable")
        {
            if (i+1 < argc)
            {
                variableName = argv[i+1];
            }
            else
            {
                std::cerr << "--variable option requires one argument." << std::endl;
                return 1;
            } 
        }
        else if (arg == "-em" || arg == "--errormode")
        {
            if (i+1 < argc)
            {
                error_mode = atoi(argv[i+1]);
            }
            else
            {
                std::cerr << "--errormode option requires one argument." << std::endl;
                return 1;
            }            
        }  
        else if (arg == "-t" || arg == "--totalsites")
        {
            if (i+1 < argc)
            {
                totalSites = atoi(argv[i+1]);
                if (totalSites < 0)
                {
                    std::cerr << "--totalsites option must be greater than 0." << std::endl;
                    return 1;
                }
                
            }
            else
            {
                std::cerr << "--totalsites option requires one argument." << std::endl;
                return 1;
            }            
        } 
        else if (arg == "-u" || arg == "--unavalsites")
        {
            if (i+1 < argc)
            {
                unavaialbleSites = atoi(argv[i+1]);
            }
            else
            {
                std::cerr << "--unavalsites option requires one argument." << std::endl;
                return 1;
            }            
        } 
        else if (arg == "-s")
        {
            if (i+1 < argc)
            {
                mgard_s_param = atof(argv[i+1]);
            }
            else
            {
                std::cerr << "-s option requires one argument." << std::endl;
                return 1;
            }            
        }    
        else if (arg == "-r" || arg == "--rawdata")
        {
            if (i+1 < argc)
            {
                rawDataFileName = argv[i+1];
            }
            else
            {
                std::cerr << "--rawdata option requires one argument." << std::endl;
                return 1;
            }            
        }     
    }

    DB* db;
    Options options;
    // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    // create the DB if it's not already present
    options.create_if_missing = true;
    // open DB
    Status s = DB::Open(options, rocksDBPath, &db);
    assert(s.ok());

    std::string varDimensionsName = variableName+":Dimensions";
    std::vector<uint32_t> dimensions;
    std::string varDimensionsResult;
    s = db->Get(ReadOptions(), varDimensionsName, &varDimensionsResult);
    assert(s.ok());  
    dimensions = UnpackVector<uint32_t>(varDimensionsResult);    
    std::cout << varDimensionsName << ", ";
    for (size_t i = 0; i < dimensions.size(); i++)
    {
        std::cout << dimensions[i] << " ";
    }
    std::cout << std::endl;

    uint32_t levels;
    std::string varLevelsName = variableName+":Levels";
    std::string varLevelsResult;
    s = db->Get(ReadOptions(), varLevelsName, &varLevelsResult);
    assert(s.ok());  
    std::unique_ptr<uint32_t> pVarLevelsResult = UnpackSingleElement<uint32_t>(varLevelsResult);
    levels = *pVarLevelsResult;
    std::cout << varLevelsName << ", " << levels << std::endl;   

    uint32_t tiers;
    std::string varTiersName = variableName+":Tiers";
    std::string varTiersResult;
    s = db->Get(ReadOptions(), varTiersName, &varTiersResult);
    assert(s.ok());  
    std::unique_ptr<uint32_t> pVarTiersResult = UnpackSingleElement<uint32_t>(varTiersResult);
    tiers = *pVarTiersResult;
    std::cout << varTiersName << ", " << tiers << std::endl;       

    std::vector<int> dataTiersECParam_k(tiers);
    std::vector<int> dataTiersECParam_m(tiers);
    std::vector<int> dataTiersECParam_w(tiers);
    std::vector<int> dataTiersECParam_hd(tiers);
    std::vector<std::vector<std::string>> dataTiersDataLocations(tiers);
    std::vector<std::vector<std::string>> dataTiersParityLocations(tiers);

    for (size_t i = 0; i < tiers; i++)
    {
        std::string varECParam_k_Name = variableName+":Tier:"+std::to_string(i)+":K";
        std::string varECParam_k_Result;
        s = db->Get(ReadOptions(), varECParam_k_Name, &varECParam_k_Result);
        assert(s.ok());  
        std::unique_ptr<int> pVarECParam_k_Result = UnpackSingleElement<int>(varECParam_k_Result);
        std::cout << varECParam_k_Name << ", " << *pVarECParam_k_Result << std::endl;   
        dataTiersECParam_k[i] = *pVarECParam_k_Result;

        std::string varECParam_m_Name = variableName+":Tier:"+std::to_string(i)+":M";
        std::string varECParam_m_Result;
        s = db->Get(ReadOptions(), varECParam_m_Name, &varECParam_m_Result);
        assert(s.ok());  
        std::unique_ptr<int> pVarECParam_m_Result = UnpackSingleElement<int>(varECParam_m_Result);
        std::cout << varECParam_m_Name << ", " << *pVarECParam_m_Result << std::endl;   
        dataTiersECParam_m[i] = *pVarECParam_m_Result;      

        std::string varECParam_w_Name = variableName+":Tier:"+std::to_string(i)+":W";
        std::string varECParam_w_Result;
        s = db->Get(ReadOptions(), varECParam_w_Name, &varECParam_w_Result);
        assert(s.ok());  
        std::unique_ptr<int> pVarECParam_w_Result = UnpackSingleElement<int>(varECParam_w_Result);
        std::cout << varECParam_w_Name << ", " << *pVarECParam_w_Result << std::endl;   
        dataTiersECParam_w[i] = *pVarECParam_w_Result;   

        std::string varECParam_hd_Name = variableName+":Tier:"+std::to_string(i)+":HD";
        std::string varECParam_hd_Result;
        s = db->Get(ReadOptions(), varECParam_hd_Name, &varECParam_hd_Result);
        assert(s.ok());  
        std::unique_ptr<int> pVarECParam_hd_Result = UnpackSingleElement<int>(varECParam_hd_Result);
        std::cout << varECParam_hd_Name << ", " << *pVarECParam_hd_Result << std::endl;   
        dataTiersECParam_hd[i] = *pVarECParam_hd_Result;  

        for (size_t j = 0; j < dataTiersECParam_k[i]; j++)
        {   
            std::string varDataLocationName = variableName+":Tier:"+std::to_string(i)+":Data:"+std::to_string(j)+":Location";
            std::string varDataLocationResult;
            s = db->Get(ReadOptions(), varDataLocationName, &varDataLocationResult);
            assert(s.ok()); 
            std::cout << varDataLocationName << ", " << varDataLocationResult << std::endl;   
            dataTiersDataLocations[i].push_back(varDataLocationResult);
        }

        for (size_t j = 0; j < dataTiersECParam_m[i]; j++)
        {   
            std::string varParityLocationName = variableName+":Tier:"+std::to_string(i)+":Parity:"+std::to_string(j)+":Location";
            std::string varParityLocationResult;
            s = db->Get(ReadOptions(), varParityLocationName, &varParityLocationResult);
            assert(s.ok()); 
            std::cout << varParityLocationName << ", " << varParityLocationResult << std::endl;   
            dataTiersParityLocations[i].push_back(varParityLocationResult);
        }
    }

    std::string variableType;
    std::string variableTypeName = variableName+":Type";
    s = db->Get(ReadOptions(), variableTypeName, &variableType);
    assert(s.ok()); 
    std::cout << variableTypeName << ", " << variableType << std::endl;  

    std::string varQueryTableShapeName = variableName+":QueryTable:Shape";   
    std::string varQueryTableShapeResult;
    s = db->Get(ReadOptions(), varQueryTableShapeName, &varQueryTableShapeResult);
    assert(s.ok());  
    std::vector<size_t> varQueryTableShape = UnpackVector<size_t>(varQueryTableShapeResult); 
    std::cout << varQueryTableShapeName << ", ";
    for (size_t i = 0; i < varQueryTableShape.size(); i++)
    {
        std::cout << varQueryTableShape[i] << " ";
    }
    std::cout << std::endl;

    std::string varQueryTableName = variableName+":QueryTable"; 
    std::string varQueryTableResult;
    s = db->Get(ReadOptions(), varQueryTableName, &varQueryTableResult);
    assert(s.ok());  
    std::vector<uint64_t> varQueryTable = UnpackVector<uint64_t>(varQueryTableResult);
    std::cout << varQueryTableName << ", " << std::endl;
    int count = 0;
    for (size_t i = 0; i < varQueryTableShape[0]; i++)
    {
        for (size_t j = 0; j < varQueryTableShape[1]; j++)
        {
            std::cout << varQueryTable[count] << " ";
            count++;
        }
        std::cout << std::endl;        
    }
    std::vector<std::vector<uint64_t>> queryTable(varQueryTableShape[0]);
    for (size_t i = 0; i < varQueryTableShape[0]; i++)
    {
        queryTable[i].insert(queryTable[i].end(), varQueryTable.begin()+i*varQueryTableShape[1], varQueryTable.begin()+i*varQueryTableShape[1]+varQueryTableShape[1]);
    }
    std::vector<std::vector<uint32_t>> level_sizes = get_level_sizes(levels, queryTable);

    std::string varSquaredErrorsShapeName = variableName+":SquaredErrors:Shape"; 
    std::string varSquaredErrorsShapeResult;
    s = db->Get(ReadOptions(), varSquaredErrorsShapeName, &varSquaredErrorsShapeResult);
    assert(s.ok());  
    std::vector<size_t> varSquaredErrorsShape = UnpackVector<size_t>(varSquaredErrorsShapeResult);
    std::cout << varSquaredErrorsShapeName << ", ";
    for (size_t i = 0; i < varSquaredErrorsShape.size(); i++)
    {
        std::cout << varSquaredErrorsShape[i] << " ";
    }
    std::cout << std::endl;

    std::string varSquaredErrorsName = variableName+":SquaredErrors";
    std::string varSquaredErrorsResult;
    s = db->Get(ReadOptions(), varSquaredErrorsName, &varSquaredErrorsResult);
    assert(s.ok());  
    std::vector<double> varSquaredErrors = UnpackVector<double>(varSquaredErrorsResult);
    std::cout << varSquaredErrorsName << ", " << std::endl;
    count = 0;
    for (size_t i = 0; i < varSquaredErrorsShape[0]; i++)
    {
        for (size_t j = 0; j < varSquaredErrorsShape[1]; j++)
        {
            std::cout << varSquaredErrors[count] << " ";
            count++;
        }
        std::cout << std::endl;        
    }

    std::vector<std::vector<double>> level_squared_errors(levels);
    size_t pos = 0;
    for (size_t i = 0; i < levels; i++)
    {
        level_squared_errors[i].insert(level_squared_errors[i].end(), varSquaredErrors.begin()+pos, varSquaredErrors.begin()+pos+varSquaredErrorsShape[1]);
        pos += varSquaredErrorsShape[1];
    }
    // for (size_t i = 0; i < level_squared_errors.size(); i++)
    // {
    //     //std::cout << "level " << i << " squared errors: ";
    //     for (size_t j = 0; j < level_squared_errors[i].size(); j++)
    //     {
    //         std::cout << level_squared_errors[i][j] << " ";
    //     }
    //     std::cout << std::endl;
    // }

    std::string varStopIndicesName = variableName+":StopIndices";
    std::string varStopIndicesResult;
    s = db->Get(ReadOptions(), varStopIndicesName, &varStopIndicesResult);
    assert(s.ok());  
    std::vector<uint8_t> stopping_indices = UnpackVector<uint8_t>(varStopIndicesResult);
    std::cout << varStopIndicesName << ", ";
    for (size_t i = 0; i < stopping_indices.size(); i++)
    {
        std::cout << +stopping_indices[i] << " ";
    }
    std::cout << std::endl;

    std::vector<uint8_t> level_num_bitplanes(levels, 0);

    if (variableType == "float")
    {
        using T = float;
        using T_stream = uint32_t;

        std::string varErrorBoundsName = variableName+":ErrorBounds";
        std::string varErrorBoundsResult;
        s = db->Get(ReadOptions(), varErrorBoundsName, &varErrorBoundsResult);
        assert(s.ok());  
        std::vector<T> level_error_bounds = UnpackVector<T>(varErrorBoundsResult);
        std::cout << varErrorBoundsName << ", ";
        for (size_t i = 0; i < level_error_bounds.size(); i++)
        {
            std::cout << level_error_bounds[i] << " ";
        }
        std::cout << std::endl;

        adios2::ADIOS adios;
        adios2::IO reader_io = adios.DeclareIO("ReaderIO");
        adios2::Engine rawdata_reader_engine =
            reader_io.Open(rawDataFileName, adios2::Mode::Read);   
        auto rawVariable = reader_io.InquireVariable<T>(variableName);
        size_t rawVariableSize = 1;
        for (size_t i = 0; i < rawVariable.Shape().size(); i++)
        {
            rawVariableSize *= rawVariable.Shape()[i];
        }
        //std::cout << "size of raw data is " << rawVariableSize << std::endl;
        std::vector<T> rawVariableData(rawVariableSize);
        rawdata_reader_engine.Get(rawVariable, rawVariableData.data(), adios2::Mode::Sync);
        rawdata_reader_engine.Close();

        auto decomposer = MDR::MGARDOrthoganalDecomposer<T>();
        // auto decomposer = MDR::MGARDHierarchicalDecomposer<T>();
        auto interleaver = MDR::DirectInterleaver<T>();
        // auto interleaver = MDR::SFCInterleaver<T>();
        // auto interleaver = MDR::BlockedInterleaver<T>();
        // auto encoder = MDR::GroupedBPEncoder<T, T_stream>();
        auto encoder = MDR::NegaBinaryBPEncoder<T, T_stream>();
        // auto encoder = MDR::PerBitBPEncoder<T, T_stream>();
        // auto compressor = MDR::DefaultLevelCompressor();
        auto compressor = MDR::AdaptiveLevelCompressor(32);
        // auto compressor = MDR::NullLevelCompressor();

        std::vector<T> reconstructedData;
        switch(error_mode)
        {
            case 1:
            {
                std::cerr << "error mode = 1 is not supported!" << std::endl;
                break;          
            } 
            default:
            {
                auto estimator = MDR::MaxErrorEstimatorOB<T>(dimensions.size());
                auto interpreter = MDR::SignExcludeGreedyBasedSizeInterpreter<MDR::MaxErrorEstimatorOB<T>>(estimator); 

                std::vector<std::vector<uint8_t>> dataTiersValues(tiers);

                std::vector<size_t> unavailableSiteList = randomly_mark_site_as_unavailable(totalSites, unavaialbleSites, 0);
                for (size_t i = 0; i < unavailableSiteList.size(); i++)
                {
                    std::cout << unavailableSiteList[i] << " ";
                }
                std::cout << std::endl;

                size_t dataTiersRecovered = 0;
                for (size_t i = 0; i < dataTiersValues.size(); i++)
                {
                    if (dataTiersECParam_m[i] < unavaialbleSites)
                    {
                        std::cout << "tier " << i << ": " << dataTiersECParam_m[i] <<  " parity chunks are not enough to recover from " << unavaialbleSites << " unavaialble sites!" << std::endl;
                        break;
                    }
                    struct ec_args args = {
                        .k = dataTiersECParam_k[i],
                        .m = dataTiersECParam_m[i],
                        .w = dataTiersECParam_w[i],
                        .hd = dataTiersECParam_hd[i],
                        .ct = CHKSUM_NONE,
                    };
                    
                    std::string varECParam_EncodedFragLen_Name = variableName+":Tier:"+std::to_string(i)+":EncodedFragmentLength";
                    std::string varECParam_EncodedFragLen_Result;
                    s = db->Get(ReadOptions(), varECParam_EncodedFragLen_Name, &varECParam_EncodedFragLen_Result);
                    assert(s.ok());  
                    std::unique_ptr<uint64_t> pVarECParam_EncodedFragLen_Result = UnpackSingleElement<uint64_t>(varECParam_EncodedFragLen_Result);
                    uint64_t encoded_fragment_len = *pVarECParam_EncodedFragLen_Result;
                    std::cout << varECParam_EncodedFragLen_Name << ", " << encoded_fragment_len << std::endl;  

                    std::string varECBackendName = variableName+":Tier:"+std::to_string(i)+":ECBackendName";
                    std::string ECBackendName;
                    s = db->Get(ReadOptions(), varECBackendName, &ECBackendName);
                    assert(s.ok()); 
                    std::cout << varECBackendName << ", " << ECBackendName << std::endl;  

                    ec_backend_id_t backendID;
                    if (ECBackendName == "flat_xor_hd")
                    {
                        backendID = EC_BACKEND_FLAT_XOR_HD;
                    }
                    else if (ECBackendName == "jerasure_rs_vand")
                    {
                        backendID = EC_BACKEND_JERASURE_RS_VAND;
                    }
                    else if (ECBackendName == "jerasure_rs_cauchy")
                    {
                        backendID = EC_BACKEND_JERASURE_RS_CAUCHY;
                    }
                    else if (ECBackendName == "isa_l_rs_vand")
                    {
                        backendID = EC_BACKEND_ISA_L_RS_VAND;
                    }
                    else if (ECBackendName == "isa_l_rs_cauchy")
                    {
                        backendID = EC_BACKEND_ISA_L_RS_CAUCHY;
                    }
                    else if (ECBackendName == "shss")
                    {
                        backendID = EC_BACKEND_SHSS;
                    }
                    else if (ECBackendName == "liberasurecode_rs_vand")
                    {
                        backendID = EC_BACKEND_LIBERASURECODE_RS_VAND;
                    }
                    else if (ECBackendName == "libphazr")
                    {
                        backendID = EC_BACKEND_LIBPHAZR;
                    }
                    else if (ECBackendName == "null")
                    {
                        backendID = EC_BACKEND_NULL;
                    }
                    else 
                    {
                        std::cerr << "the specified EC backend is not supported!" << std::endl;
                        return 1;
                    }

                    int rc = 0;
                    int desc = -1;
                    uint64_t decoded_data_len = 0;
                    char *decoded_data = NULL;
                    char **avail_frags = NULL;
                    int num_avail_frags = 0;
                    avail_frags = (char **)malloc((dataTiersECParam_k[i] + dataTiersECParam_m[i]) * sizeof(char *));
                    if (avail_frags == NULL)
                    {
                        num_avail_frags = -1;
                        std::cerr << "memory allocation for avail_frags failed!" << std::endl;
                        return 1;
                    }
                    desc = liberasurecode_instance_create(backendID, &args);
                    if (-EBACKENDNOTAVAIL == desc) 
                    {
                        std::cerr << "backend library not available!" << std::endl;
                        return 1;
                    } else if ((args.k + args.m) > EC_MAX_FRAGMENTS) 
                    {
                        assert(-EINVALIDPARAMS == desc);
                        std::cerr << "invalid parameters!" << std::endl;
                        return 1;
                    } else
                    {
                        assert(desc > 0);
                    }  

                    for (size_t j = 0; j < dataTiersECParam_k[i]; j++)
                    {
                        /* check if data chunks are avaialble */
                        if (std::find(unavailableSiteList.begin(), unavailableSiteList.end(), j) != unavailableSiteList.end())
                        {
                            std::cout << "cannot access data chunk " << j << " since site " << j << " is unavailable! skip!" << std::endl;
                            continue;
                        }
                        adios2::Engine data_reader_engine =
                            reader_io.Open(dataTiersDataLocations[i][j], adios2::Mode::Read); 
                        std::string varDataValuesName = variableName+":Tier:"+std::to_string(i)+":Data:"+std::to_string(j);
                        auto varDataValues = reader_io.InquireVariable<char>(varDataValuesName);
                        // for (size_t k = 0; k < varDataValues.Shape().size(); k++)
                        // {
                        //     std::cout << varDataValues.Shape()[k] << " ";
                        // }
                        // std::cout << std::endl;
                        //std::vector<char> encodedValues(varDataValues.Shape()[0]);
                        avail_frags[num_avail_frags] = (char *)malloc(varDataValues.Shape()[0]*sizeof(char));
                        data_reader_engine.Get(varDataValues, avail_frags[num_avail_frags], adios2::Mode::Sync);
                        data_reader_engine.Close();
                        //avail_frags[j] = encodedValues.data();
                        num_avail_frags++;
                    }
                    for (size_t j = 0; j < dataTiersECParam_m[i]; j++)
                    {
                        /* check if parity chunks are avaialble */
                        if (std::find(unavailableSiteList.begin(), unavailableSiteList.end(), j+dataTiersECParam_k[i]) != unavailableSiteList.end())
                        {
                            std::cout << "cannot access parity chunk " << j << " since site " << j+dataTiersECParam_k[i] << " is unavailable! skip!" << std::endl;
                            continue;
                        }
                        adios2::Engine parity_reader_engine =
                            reader_io.Open(dataTiersParityLocations[i][j], adios2::Mode::Read); 
                        std::string varParityValuesName = variableName+":Tier:"+std::to_string(i)+":Parity:"+std::to_string(j);
                        auto varParityValues = reader_io.InquireVariable<char>(varParityValuesName);
                        // for (size_t k = 0; k < varParityValues.Shape().size(); k++)
                        // {
                        //     std::cout << varParityValues.Shape()[k] << " ";
                        // }
                        // std::cout << std::endl;
                        //std::vector<char> encodedValues(varParityValues.Shape()[0]);
                        avail_frags[num_avail_frags] = (char *)malloc(varParityValues.Shape()[0]*sizeof(char));
                        parity_reader_engine.Get(varParityValues, avail_frags[num_avail_frags], adios2::Mode::Sync);
                        parity_reader_engine.Close();
                        //avail_frags[j+storageTiersECParam_k[i]] = encodedValues.data();
                        num_avail_frags++;
                    }    
                    assert(num_avail_frags > 0);

                    rc = liberasurecode_decode(desc, avail_frags, num_avail_frags,
                                            encoded_fragment_len, 1,
                                            &decoded_data, &decoded_data_len);   
                    assert(0 == rc);

                    uint8_t *tmp = static_cast<uint8_t*>(static_cast<void *>(decoded_data));  
                    std::vector<uint8_t> oneTierDecodedData(tmp, tmp+decoded_data_len);
                    dataTiersValues[i] = oneTierDecodedData;

                    rc = liberasurecode_decode_cleanup(desc, decoded_data);
                    assert(rc == 0);

                    assert(0 == liberasurecode_instance_destroy(desc));

                    free(avail_frags);

                    dataTiersRecovered++;                

                }
                std::cout << dataTiersRecovered << " data tiers recovered!" << std::endl;
                if (dataTiersRecovered == 0)
                {
                    std::cerr << "no data tier is recovered! all data is unavailable!" << std::endl;
                    return 1;
                }

                uint8_t target_level = level_error_bounds.size()-1;
                std::vector<std::vector<const uint8_t*>> level_components(levels);
                for (size_t j = 0; j < queryTable.size(); j++)
                {
                    //std::cout << j << ": " << queryTable[j][0] << ", " << queryTable[j][1] << ", " << queryTable[j][2] << ", " << queryTable[j][3] << ", " << queryTable[j][5] << std::endl;
                    if (queryTable[j][2] == dataTiersRecovered)
                    {
                        break;
                    }
                    
                    uint8_t * buffer = (uint8_t *) malloc(queryTable[j][4]);
                    std::copy(dataTiersValues[queryTable[j][2]].begin()+queryTable[j][3], dataTiersValues[queryTable[j][2]].begin()+queryTable[j][3]+queryTable[j][4], buffer);
                    level_components[queryTable[j][0]].push_back(buffer);
                    level_num_bitplanes[queryTable[j][0]]++;
                }
                int skipped_level = 0;
                for(size_t j = 0; j <= target_level; j++)
                {
                    if(level_num_bitplanes[target_level-j] != 0)
                    {
                        skipped_level = j;
                        break;
                    }
                }
                target_level -= skipped_level;
                auto level_dims = MDR::compute_level_dims(dimensions, target_level);
                auto reconstruct_dimensions = level_dims[target_level];
                uint32_t num_elements = 1;
                for(const auto& dim:reconstruct_dimensions)
                {
                    num_elements *= dim;
                }

                reconstructedData = std::vector<T>(num_elements, 0);
                auto level_elements = MDR::compute_level_elements(level_dims, target_level);

                std::vector<uint32_t> dims_dummy(reconstruct_dimensions.size(), 0);
                for(size_t j = 0; j <= target_level; j++)
                {
                    // std::cout << "level " << j << " components size: "<< level_components[j].size() << std::endl;
                    // for (size_t k = 0; k < level_components[j].size(); k++)
                    // {
                    //     std::cout << j << ", " << k << ": ";
                    //     for (size_t l = 0; l < 20; l++)
                    //     {
                    //         std::cout << +level_components[j][k][l] << " ";
                    //     }
                    //     std::cout << std::endl;
                    // }
                    
                    compressor.decompress_level(level_components[j], level_sizes[j], 0, level_num_bitplanes[j], stopping_indices[j]);

                    int level_exp = 0;
                    frexp(level_error_bounds[j], &level_exp);
                    auto level_decoded_data = encoder.progressive_decode(level_components[j], level_elements[j], level_exp, 0, level_num_bitplanes[j], j);
                    compressor.decompress_release();
                    const std::vector<uint32_t>& prev_dims = (j == 0) ? dims_dummy : level_dims[j-1];
                    interleaver.reposition(level_decoded_data, reconstruct_dimensions, level_dims[j], prev_dims, reconstructedData.data());
                    free(level_decoded_data);
                    //std::cout << " pass" << std::endl;
                }

                decomposer.recompose(reconstructedData.data(), reconstruct_dimensions, target_level);
                MGARD::print_statistics(rawVariableData.data(), reconstructedData.data(), rawVariableData.size()); 
                
            }
        }

    }

    delete db;

}